local ipc = require "ipc"
local map = require "map"

local FALLBACK_PORT_NUM = 56767

function love.load(arg)
    love.window.setMode(1280, 720, { resizable = true, vsync = true })

    ipc.connect(tonumber(arg[1]) or FALLBACK_PORT_NUM)

    local cities = ipc.fetch_cities()
    ipc.client:settimeout(0)

    MAP_IMG = map.fetch(map.build_url(cities))

    local width, height = love.graphics.getDimensions()

    -- Pre-render map image to a canvas to avoid scaling it every frame
    MAP_CANVAS = love.graphics.newCanvas(width, height)

    -- Point to the map canvas as the current canvas, start drawing map canvas
    love.graphics.setCanvas(MAP_CANVAS)

    -- Clear the canvas before drawing the map image to make sure it's blank
    love.graphics.clear()

    local scale_x, scale_y = width / MAP_IMG:getWidth(), height / MAP_IMG:getHeight()
    love.graphics.draw(MAP_IMG, 0, 0, 0, scale_x, scale_y)

    -- Point to the screen as the current canvas, finish drawing map canvas
    love.graphics.setCanvas()
end

function love.update()
    CURR_TOUR = ipc.fetch_tour()
end

function love.draw()
    -- Reset background color to white to avoid tinting stuff when drawing
    love.graphics.setColor(1, 1, 1)

    -- Draw pre-rendered map image scaled to fill up the entire window (background)
    love.graphics.draw(MAP_CANVAS, 0, 0)

    -- Draw tour on map each generation
    if CURR_TOUR then
    end
end

function love.resize(width, height)
    MAP_CANVAS = love.graphics.newCanvas(width, height)

    -- Point to the map canvas as the current canvas, start drawing map canvas
    love.graphics.setCanvas(MAP_CANVAS)

    -- Clear the canvas before drawing the map image to make sure it's blank
    love.graphics.clear()

    -- Scale the map image to fill up the entire NEW-SIZED window
    local scale_x, scale_y = width / MAP_IMG:getWidth(), height / MAP_IMG:getHeight()
    love.graphics.draw(MAP_IMG, 0, 0, 0, scale_x, scale_y)

    -- Point to the screen as the current canvas, finish drawing map canvas
    love.graphics.setCanvas()
end

function love.quit()
    ipc.close()
end