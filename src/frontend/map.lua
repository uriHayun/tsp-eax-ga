local function log(s) love.filesystem.append("debug.log", os.date("%X ") .. s .. "\n") end

local mercator = require "pseudo_mercator"

local https = require "https"
local sha1 = require "sha1"
local url = require "socket.url"

local M =  {}

local MAP_H, MAP_W = 640, 640
local MAP_SCALE = 2

local MAP_SIZE_ZOOM0 = 256  -- Points-wide for zoom 0
local MAX_ZOOM = 18         -- The max zoom Google Maps Static API allows
local PADDING = 1.1         -- Extra 10% room - so no city could lie on map edge

local function build_markers(cities)
    -- Small, red markers (pins) for each city of the TSP
    local markers = "size:tiny|color:red"

    for _, city in ipairs(cities) do
        markers = markers .. string.format("|%.5f,%.5f", city.lat, city.lng)
    end

    return markers
end

local get_bounding_map_frame = function(cities_ordered_pair)
    local bounds = {
        min_x = math.huge,  max_x = -math.huge,
        min_y = math.huge,  max_y = -math.huge,
    }

    for _, city in ipairs(cities_ordered_pair) do
        bounds.min_x = math.min(bounds.min_x, city.x)
        bounds.max_x = math.max(bounds.max_x, city.x)
        bounds.min_y = math.min(bounds.min_y, city.y)
        bounds.max_y = math.max(bounds.max_y, city.y)
    end

    -- Representing map frame's 4 corners
    return bounds
end

local function get_map_center(bounds)
    local midd_x, midd_y = (bounds.min_x + bounds.max_x) / 2, (bounds.min_y + bounds.max_y) / 2

    return mercator.unproject(midd_x, midd_y)
end

local function get_map_zoom(bounds)
    local frame_w, frame_h = (bounds.max_x - bounds.min_x) * PADDING, (bounds.max_y - bounds.min_y) * PADDING

    -- Begin most zoomed in, zoom out until all cities are in padded frame
    for zoom = MAX_ZOOM, 0, -1 do
        local map_size = MAP_SIZE_ZOOM0 * 2^zoom

        if frame_w * map_size <= MAP_W and frame_h * map_size <= MAP_H then
            return zoom
        end
    end

    return 0
end

function M.get_view(cities_coords)
    local bounds = get_bounding_map_frame(mercator.project_all(cities_coords))

    local center_lng, center_lat = get_map_center(bounds)

    return {
        center_lng = center_lng, center_lat = center_lat,
        zoom = get_map_zoom(bounds)
    }
end

-- Sign a request URL with a Google URL signing secret
local function sign_url(input_url, secret)
    assert(input_url and secret, 'Both "input_url", and "secret" are required')

    -- path + query only (strip scheme and host)
    local url_to_sign = input_url:match("^https?://[^/]+(/.*)$")
    assert(url_to_sign, "Couldn't parse the URL")

    -- urlsafe base64 -> standard base64 -> raw key bytes
    local std = secret:gsub("%-", "+"):gsub("_", "/")
    local key = love.data.decode("string", "base64", std)

    -- raw binary HMAC-SHA1
    local sig = sha1.hmac_binary(key, url_to_sign)

    -- raw bytes -> standard base64 -> urlsafe base64
    local b64 = love.data.encode("string", "base64", sig)
    local encoded = b64:gsub("%+", "-"):gsub("/", "_")

    return input_url .. "&signature=" .. encoded
end

local max_url_len = 16384

function M.build_url(cities, view)
    local api_key = os.getenv("GOOGLE_MAPS_STATIC_API_KEY_PRIMARY") or os.getenv("GOOGLE_MAPS_STATIC_API_KEY_ALTERNATIVE") or ""

    if api_key == "" then  -- Using api_key = ... or "" - so key cannot be nil
        error("GOOGLE_MAPS_STATIC_API_KEY_PRIMARY/GOOGLE_MAPS_STATIC_API_KEY_ALTERNATIVE environment variables weren't found")
    end

    local map_params = {
        center = string.format("%.5f,%.5f", view.center_lat, view.center_lng),
        zoom = view.zoom,
        size = string.format("%dx%d", MAP_W, MAP_H),
        scale = MAP_SCALE,
        markers = build_markers(cities),
        maptype = "roadmap",
        key = api_key
    }

    local map_img_url = "https://maps.googleapis.com/maps/api/staticmap?"

    -- Insert map parameters into the URL
    for key, value in pairs(map_params) do
        map_img_url = map_img_url .. string.format("%s=%s&", key, url.escape(tostring(value)))
    end

    local signing_secret = os.getenv("GOOGLE_MAPS_STATIC_API_SIGNING_SECRET")
    if not signing_secret or signing_secret == "" then
        error("GOOGLE_MAPS_STATIC_API_SIGNING_SECRET environment variable wasn't found")
    end

    local signed_url = sign_url(map_img_url:gsub("&$", ""),  -- ":gsub" removes trailing "&" if present
        signing_secret)

    if #signed_url > max_url_len then
        return nil, string.format("URL too long (> %d): too many cities for one map URL", max_url_len)
    end

    return signed_url
end

local http_ok = 200

local png_sig = love.filesystem.read("images/window-icon/window_icon.png", 8) or "\137PNG\r\n\26\n"

-- Returns whether an image is of type PNG by checking if its 1st 8 bytes are the PNG image's signature
local is_png = function(img) return img:sub(1, 8) == png_sig end

function M.fetch(url_map)
    log("Map requesting begins")

    local valid, status_code, body = pcall(https.request, url_map)
    if not valid then
        return nil, "Request error: " .. tostring(status_code)
    end

    log("Map request done: status=" .. tostring(status_code) .. " bytes=" .. tostring(body and #body))

    if status_code ~= http_ok then
        if is_png(tostring(body)) then
            love.filesystem.write("error.png", body)
        end
        local snippet = tostring(body):sub(1, 200):gsub("[^%g ]", "?")
        return nil, "HTTP code: " .. tostring(status_code) .. " " .. snippet
    end

    if type(body) ~= "string" or not is_png(body) then
        return nil, "Response isn't PNG"
    end

    -- Since Google's terms mostly don't allow caching their maps, request a new map each run
    return love.graphics.newImage(love.filesystem.newFileData(body, "map.png"))
end

function M.get_map_placement(map_img)
    local win_w, win_h = love.graphics.getDimensions()
    local img_w, img_h = map_img:getWidth(), map_img:getHeight()

    -- Same scale for both axes - prevent image distorting:
    -- largest factor which fits the whole image
    local scale = math.min(win_w / img_w, win_h / img_h)

    local offset_x, offset_y = (win_w - img_w*scale) / 2, (win_h - img_h*scale) / 2

    return scale, offset_x, offset_y
end

return M