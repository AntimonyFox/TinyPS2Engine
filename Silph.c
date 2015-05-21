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

#include <kernel.h>
#include <stdlib.h>
#include <tamtypes.h>
#include <math3d.h>

#include <packet.h>

#include <dma_tags.h>
#include <gif_tags.h>
#include <gs_psm.h>

#include <dma.h>

#include <graph.h>

#include <draw.h>
#include <draw3d.h>

// Pad libraries
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <math.h>

#include <libpad.h>

//#include <debug.h>


#include "Utilities.c"
#include "square_data.c"



#include "bg.c"
extern unsigned char bg[];
#include "flower.c"
extern unsigned char flower[];
#include "player_0_0.c"
extern unsigned char player_0_0[];




static pad_buffer padBuf;

VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };


canvas create_canvas(int width, int height)
{

    canvas c;

    framebuffer_t *frame = &c.frame;
    zbuffer_t *z = &c.z;
    memory *m = &c.memory;

    // Define a 32-bit 640x512 framebuffer.
    frame->width = width;
    frame->height = height;
    frame->mask = 0;
    frame->psm = GS_PSM_32;
    frame->address = graph_vram_allocate(frame->width, frame->height, frame->psm, GRAPH_ALIGN_PAGE);

    // Enable the zbuffer.
    z->enable = DRAW_ENABLE;
    z->mask = 0;
    z->method = ZTEST_METHOD_GREATER_EQUAL;
    z->zsm = GS_ZBUF_32;
    z->address = graph_vram_allocate(frame->width,frame->height,z->zsm, GRAPH_ALIGN_PAGE);

    // Create workspace
    //TODO: set the vertex_count to the largest geometry's vertex count
    m->temp_vertices = make_buffer(sizeof(VECTOR), vertex_count);
    m->verts  = make_buffer(sizeof(u64), vertex_count);
    m->colors = make_buffer(sizeof(u64), vertex_count);
    m->coordinates = make_buffer(sizeof(u64), vertex_count);
    //TODO: what does this do?
//    m->color.r = 0x80;
//    m->color.g = 0x80;
//    m->color.b = 0x80;
//    m->color.a = 0x80;
//    m->color.q = 1.0f;

    // Define the triangle primitive we want to use.
    prim_t *p = &c.prim;
    p->type = PRIM_TRIANGLE;
    p->shading = PRIM_SHADE_GOURAUD;
    p->mapping = DRAW_ENABLE;
    p->fogging = DRAW_DISABLE;
    p->blending = DRAW_DISABLE;         //this must be disabled to correctly enable transparency
    p->antialiasing = DRAW_DISABLE;
    p->mapping_type = PRIM_MAP_ST;
    p->colorfix = PRIM_UNFIXED;

    // Set sprite geometry settings (pulled from square_data.c)
    geometry *g = &c.sprite_geometry;
    g->vertex_count = vertex_count;
    g->vertices = vertices;
    g->colors = colors;
    g->coordinates = coordinates;
    g->index_count = points_count;
    g->indices = points;

    // Create double-buffer
    c.buffers[0] = create_packet(3000);
    c.buffers[1] = create_packet(3000);

    // Initialize the screen and tie the first framebuffer to the read circuits.
    graph_initialize(frame->address, frame->width, frame->height, frame->psm, 0, 0);

    // Register canvas with the coprocessor
    register_canvas(&c);

    return c;

}

int render(canvas *c)
{

    // Matrices to set up the 3D environment and camera
    MATRIX *CAM = &c->memory.CAM;


    // Create perspective
    set_frustum(c, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

    wait();



    // Load textures
    sprite bg_sprite = load_sprite(bg, 512, 512, 512, 384, 0, 64);
//    sprite flower_sprite = load_sprite(flower, 256, 256, 240, 149, 9, 59);
    sprite player_0_0_s = load_sprite(player_0_0, 128, 64, 89, 44, 19, 10);

    // Create entities
    entity e_bg = create_entity(&bg_sprite);
    e_bg.scale[0] = 43.5f;
    e_bg.scale[1] = 43.5f;

    entity e_player_0_0 = create_entity(&player_0_0_s);
    e_player_0_0.scale[0] = 5;
    e_player_0_0.scale[1] = 5;



    // Set up pads
    loadPadModules();

    int port = 0;
    int slot = 0;
    pad pad = initialize_pad(port, slot, padBuf);


    int success = 0;
    double speed = 1.0f;

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

        if (success != 0) {
            double lX = pad.buttons.ljoy_h / 127.0f - 1;
            double lY = -(pad.buttons.ljoy_v / 127.0f - 1);

            double rX = pad.buttons.rjoy_h / 127.0f - 1;
            double rY = -(pad.buttons.rjoy_v / 127.0f - 1);
            if (hypot(rX, rY) > 0.4f)
                e_player_0_0.angle = atan2(rY, rX);

            if (fabs(lX) > 0.4f)
                e_player_0_0.position[0] += lX * speed;
            if (fabs(lY) > 0.4f)
                e_player_0_0.position[1] += lY * speed;



            // Directions
//            if (new_pad & PAD_LEFT)
//                e_player_0_0.angle = (float)atan2(0, -1);
//            if (new_pad & PAD_RIGHT)
//                e_player_0_0.angle = (float)atan2(0, 1);
//            if (new_pad & PAD_UP)
//                e_player_0_0.angle = (float)atan2(1, 0);
//            if (new_pad & PAD_DOWN)
//                e_player_0_0.angle = (float)atan2(-1, 0);


//            scr_printf("%f\t%f\t%f\n", rX, rY, sqrt(pow(rX,2) + pow(rY,2)));
        }
        drawObject(c, &e_player_0_0);



        // End drawing
        commit_canvas(c);

    }

    // ~~ Free if we're done with the for loop, but we never will be
//    packet_free(packets[0]);
//    packet_free(packets[1]);

    return 0;

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
