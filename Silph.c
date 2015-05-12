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

#include "Utilities.c"
#include "mesh_data.c"

VECTOR object_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR object_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };

void init_dma()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

void create_canvas(canvas *c, int width, int height)
{

    framebuffer_t *frame = &c->frame;
    zbuffer_t *z = &c->z;

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

    // Initialize the screen and tie the first framebuffer to the read circuits.
    graph_initialize(frame->address, frame->width, frame->height, frame->psm, 0, 0);

    // Register canvas with the coprocessor
    register_canvas(c);

}

int render(canvas *c)
{

    int i;
    int context = 0;

    // Matrices to setup the 3D environment and camera
    MATRIX P;
    MATRIX MV;
    MATRIX CAM;
    MATRIX FINAL;

    //These things should maybe be hidden
    VECTOR *temp_vertices;
    xyz_t   *verts;
    color_t *colors;

    prim_t prim;
    color_t color;

    // The data packets for double buffering dma sends.
    packet_t *packets[2];
    packet_t *current;
    qword_t *q;
    qword_t *dmatag;
    wand w;

    packets[0] = create_packet(100);
    packets[1] = create_packet(100);

    temp_vertices = make_buffer(sizeof(VECTOR), vertex_count);
    verts  = make_buffer(sizeof(vertex_t), vertex_count);
    colors = make_buffer(sizeof(color_t), vertex_count);

    // Define the triangle primitive we want to use.
    prim.type = PRIM_TRIANGLE;
    prim.shading = PRIM_SHADE_GOURAUD;
    prim.mapping = DRAW_DISABLE;
    prim.fogging = DRAW_DISABLE;
    prim.blending = DRAW_DISABLE;
    prim.antialiasing = DRAW_ENABLE;
    prim.mapping_type = PRIM_MAP_ST;
    prim.colorfix = PRIM_UNFIXED;


    // ~~ White
    color.r = 0x80;
    color.g = 0x80;
    color.b = 0x80;
    color.a = 0x80;
    color.q = 1.0f;

//    frustum(P, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);
    frustum(P, graph_aspect_ratio(), -3.00f, 3.00f, -5.00f, 5.00f, 1.00f, 2000.00f);

    wait();

    // The main loop...
    for (;;)
    {
        //TODO: call it buffer instead or something
        current = packets[context];

//        create_wand(&w, current);
        dmatag = current->data;
        q = dmatag;
        q++;
//        clear(&w, c);
        q = clear(q, c);
//        use_wand(&w);
        DMATAG_END(dmatag, (q-current->data)-1, 0, 0, 0);
        dma_wait_fast();
        dma_channel_send_chain(DMA_CHANNEL_GIF, current->data, q - current->data, 0, 0);



        dmatag = current->data;
        q = dmatag;
        q++;

        //Object transformations
        object_position[0] = -15.000f;
        object_rotation[0] += 0.008f; //while (object_rotation[0] > 3.14f) { object_rotation[0] -= 6.28f; }
        object_rotation[1] += 0.012f; //while (object_rotation[1] > 3.14f) { object_rotation[1] -= 6.28f; }

        create_local_world(MV, object_position, object_rotation);
        create_world_view(CAM, camera_position, camera_rotation);

        create_local_screen(FINAL, MV, CAM, P);

        calculate_vertices(temp_vertices, vertex_count, vertices, FINAL);

        draw_convert_xyz(verts, 2048, 2048, 32, vertex_count, (vertex_f_t*)temp_vertices);
        draw_convert_rgbq(colors, vertex_count, (vertex_f_t*)temp_vertices, (color_f_t*)colours, 0x80);

        //drawElements
        q = draw_prim_start(q, 0, &prim, &color);
        for(i = 0; i < points_count; i++)
        {
            q->dw[0] = colors[points[i]].rgbaq;
            q->dw[1] = verts[points[i]].xyz;
            q++;
        }
        q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);

        q = draw_finish(q);
        DMATAG_END(dmatag, (q-current->data)-1, 0, 0, 0);
        dma_wait_fast();
        dma_channel_send_chain(DMA_CHANNEL_GIF, current->data, q - current->data, 0, 0);




//        dmatag = current->data;
//        q = dmatag;
//        q++;
//
//        //Object transformations
//        object_position[0] = 15.000f;
//        object_rotation[0] += 0.008f; //while (object_rotation[0] > 3.14f) { object_rotation[0] -= 6.28f; }
//        object_rotation[1] += 0.012f; //while (object_rotation[1] > 3.14f) { object_rotation[1] -= 6.28f; }
//
//        create_local_world(MV, object_position, object_rotation);
//        create_world_view(CAM, camera_position, camera_rotation);
//
//        create_local_screen(FINAL, MV, CAM, P);
//
//        calculate_vertices(temp_vertices, vertex_count, vertices, FINAL);
//
//        draw_convert_xyz(verts, 2048, 2048, 32, vertex_count, (vertex_f_t*)temp_vertices);
//        draw_convert_rgbq(colors, vertex_count, (vertex_f_t*)temp_vertices, (color_f_t*)colours, 0x80);
//
//        //drawElements
//        q = draw_prim_start(q, 0, &prim, &color);
//        for(i = 0; i < points_count; i++)
//        {
//            q->dw[0] = colors[points[i]].rgbaq;
//            q->dw[1] = verts[points[i]].xyz;
//            q++;
//        }
//        q = draw_prim_end(q, 2, DRAW_RGBAQ_REGLIST);
//
//        q = draw_finish(q);
//        DMATAG_END(dmatag, (q-current->data)-1, 0, 0, 0);
//        dma_wait_fast();
//        dma_channel_send_chain(DMA_CHANNEL_GIF, current->data, q - current->data, 0, 0);




        context ^= 1;

        draw_wait_finish();
        graph_wait_vsync();

    }

    // ~~ Free if we're done with the for loop, but we never will be
    packet_free(packets[0]);
    packet_free(packets[1]);

    return 0;

}

void startup(int width, int height)
{

    init_dma();

    //TODO: can this be the one to produce the canvas?
    canvas c;
    create_canvas(&c, width, height);
    clear_color(&c, 0x80, 0, 0x80);

    // Render the cube
    render(&c);

    // Sleep
    SleepThread();

}
