local ltn12 = require "ltn12"
local https = require "ssl.https"
local url = require "socket.url"

local M =  {}

local function build_markers(cities)
    -- Small, red markers (pins) for each city of the TSP
    local markers = "size:tiny|color:red"

    for _, city in ipairs(cities) do
        markers = markers .. string.format("|%.5f,%.5f", city.lat, city.lng)
    end

    return markers
end

function M.build_url(cities)
    local map_params = {
        size = string.format("%dx%d", love.graphics.getWidth(), love.graphics.getHeight()),
        scale = 2,
        markers = build_markers(cities),
        maptype = "roadmap",
        key = ""
    }
    local map_img_url = "https://maps.googleapis.com/maps/api/staticmap?"

    -- Insert map parameters into the URL
    for key, value in pairs(map_params) do
        map_img_url = map_img_url .. string.format("%s=%s&", key, url.escape(tostring(value)))
    end

    return (map_img_url:gsub("&$", ""))  -- Remove trailing "&" if present
end

function M.fetch(url)
    local map_filename = "images/map.png"

    local map_file = assert(io.open(map_filename, "wb"))

    local body, status_code, _, status_msg = https.request({
        url = url,
        sink = ltn12.sink.file(map_file)
    })

    map_file:close()

    if not body or (status_code ~= 200 and status_code ~= 201) then
        os.remove(map_filename)
        error("Failed to download map image: " .. tostring(status_msg or status_code))
    end

    return love.graphics.newImage(map_filename)
end

return M