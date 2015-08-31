/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2005 Dan Peori <peori@oopo.net>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/

#include "Utilities.c"


VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };


void update_pellet(entity *p)
{
    p->angle += 0.012f;
}

void render(canvas *c)
{

    // Matrices to set up the 3D environment and camera
    MATRIX *CAM = &c->memory.CAM;

    wait();



    // Load textures
    sprite bg_sprite = load_sprite(bg, 512, 512, 512, 356, 0, 78);
    sprite player_0_0_s = load_sprite(player_0_0, 128, 64, 89, 44, 19, 10);
    sprite yellow_pellet_s = load_sprite(yellow_pellet, 16, 16, 16, 16, 0, 0);

    // Create entities
    entity e_bg = create_entity(&bg_sprite);
    set_size(&e_bg, 640, 445);

    entity e_player_0_0 = create_entity(&player_0_0_s);
    set_width(&e_player_0_0, 50);

    entity yellow_pellet_e = create_entity(&yellow_pellet_s);



    // Set up pads
    loadPadModules();

    int port = 0;
    int slot = 0;
    pad pad = create_pad(port, slot);


    int success = 0;
    double speed = 25.0f;

    // The main loop...
    for (;;)
    {

        // Get new controller inputs
        success = update_pad(&pad);



        // Begin drawing
        ready_canvas(c);

        // Clear screen
        clear(c);

        // Update camera
        create_CAM(*CAM, camera_position, camera_rotation);



        // Draw objects
        drawObject(c, &e_bg);

        VECTOR newPos;

        if (success != 0) {
            double lX = pad.buttons.ljoy_h / 127.0f - 1;
            double lY = -(pad.buttons.ljoy_v / 127.0f - 1);

            double rX = pad.buttons.rjoy_h / 127.0f - 1;
            double rY = -(pad.buttons.rjoy_v / 127.0f - 1);
            if (hypot(rX, rY) > 0.4f)
                e_player_0_0.angle = atan2(rY, rX);

            // Request new position
            if (fabs(lX) > 0.4f)
                newPos[0] += lX * speed;
            if (fabs(lY) > 0.4f)
                newPos[1] += lY * speed;


            // Screen info
            float aspect = 1.44f;
            float hW = 640.0f / 2.0f;
            float hH = hW / aspect;

            // Check boundaries (rectangle)
//            newPos[0] = (newPos[0] < -hW) ? -hW : (newPos[0] > hW) ? hW : newPos[0];
//            newPos[1] = (newPos[1] < -hH) ? -hH : (newPos[1] > hH) ? hH : newPos[1];

            // Check boundaries (circle)
//            if (hypot(newPos[0], newPos[1]) > 200) {
//                double bearing = atan2(newPos[1], newPos[0]);
//                newPos[0] = 200 * cos(bearing);
//                newPos[1] = 200 * sin(bearing);
//            }

            // Check boundaries (ellipse)
            float shipRad = 25.0f;
            if (pow(newPos[0], 2) / pow(hW - shipRad, 2) + pow(newPos[1], 2) / pow(hH - shipRad, 2) > 1.0f) {
                float proj = newPos[0] / aspect;
                double bearing = atan2(newPos[1], proj);
                newPos[0] = (hW - shipRad) * cos(bearing);
                newPos[1] = (hH - shipRad) * sin(bearing);
            }

            e_player_0_0.position[0] = newPos[0];
            e_player_0_0.position[1] = newPos[1];


            if (pad.new_pad & PAD_R1)
                start_big_motor(&pad);
            if (pad.new_pad & PAD_L1)
                stop_big_motor(&pad);
            if (pad.new_pad & PAD_L3)
                run_big_motor(&pad, 255, 1.0f);

            if (pad.new_pad & PAD_R2)
                start_small_motor(&pad);
            if (pad.new_pad & PAD_L2)
                stop_small_motor(&pad);
            if (pad.new_pad & PAD_R3)
                run_small_motor(&pad, 0.25f);

//            scr_printf("%f\t%f\t%f\n", rX, rY, sqrt(pow(rX,2) + pow(rY,2)));
        }
        drawObject(c, &e_player_0_0);

        update_pellet(&yellow_pellet_e);
        drawObject(c, &yellow_pellet_e);



        // End drawing
        commit_canvas(c);

    }

}

void startup(int width, int height)
{

    init_dma();

    canvas c = create_canvas(width, height);
    set_clear_color(&c, 0x80, 0, 0x80);

    // Render the cube
    render(&c);

    // Sleep
    SleepThread();

}
