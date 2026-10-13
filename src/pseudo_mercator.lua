local M = {}

-- Max latitude Pseudo-Mercator can represent
local MAX_LAT = 85.05113

-- Pseudo-mercator: converts earth coordinates (lng, lat) to map ordered pairs (x, y)
-- Earth is considered a sphere - not its true ellipsoid shape
local function project(lng, lat)
    lat = math.max(-MAX_LAT, math.min(MAX_LAT, lat))
    local phi = math.rad(lat)

    local x = (lng + 180) / 360
    local y = 0.5 - math.log(math.tan(math.pi / 4 + phi / 2)) / (2 * math.pi)

    return x, y
end

function M.project_all(cities_coords)
    local cities_ordered_pair = {}

    for i, city in ipairs(cities_coords) do
        local x, y = project(city.lng, city.lat)
        cities_ordered_pair[i] = { x = x, y = y }
    end
    return cities_ordered_pair
end

function M.unproject(x, y)
    local lng = x * 360 - 180
    local lat = math.deg(2 * math.atan(math.exp((0.5 - y) * 2 * math.pi)) - math.pi/2)

    return lat, lng
end

return M