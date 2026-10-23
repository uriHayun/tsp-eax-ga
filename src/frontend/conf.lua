function love.conf(t)
    t.identity = "tsp-eax-ga"

    t.window.title = "TSP Visualizer.. CS50x Final Project: TSP-EAX-GA"
    t.window.icon = "images/window_icon.png"

    t.version = "11.5"
    t.window.width = 1280
    t.window.height = 720
    t.window.resizable = true
    t.window.vsync = true

    t.modules.joystick = false
    t.modules.physics = false
    t.modules.audio = false
    t.modules.sound = false
    t.modules.video = false
end