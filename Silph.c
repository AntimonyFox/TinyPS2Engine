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
#include "square_data.c"



#include "bg.c"
extern unsigned char bg[];



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
    memory *m = &c->memory;

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

    // Create double-buffer
    c->buffers[0] = create_packet(3000);
    c->buffers[1] = create_packet(3000);

    // Initialize the screen and tie the first framebuffer to the read circuits.
    graph_initialize(frame->address, frame->width, frame->height, frame->psm, 0, 0);

    // Register canvas with the coprocessor
    register_canvas(c);

}

void load_texture(texbuffer_t *texbuf, char *)
{

    packet_t *packet = create_packet(50);

    qword_t *q = packet->data;

    q = packet->data;

    q = draw_texture_transfer(q, bg, 256, 256, GS_PSM_24, texbuf->address, texbuf->width);
    q = draw_texture_flush(q);

    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0,0);
    dma_wait_fast();

    packet_free(packet);

}

void setup_texture(texbuffer_t *texbuf)
{

    packet_t *packet = packet_init(10, PACKET_NORMAL);

    qword_t *q = packet->data;

    // Using a texture involves setting up a lot of information.
    clutbuffer_t clut;

    lod_t lod;

    lod.calculation = LOD_USE_K;
    lod.max_level = 0;
    lod.mag_filter = LOD_MAG_NEAREST;
    lod.min_filter = LOD_MIN_NEAREST;
    lod.l = 0;
    lod.k = 0;

    texbuf->info.width = draw_log2(256);
    texbuf->info.height = draw_log2(256);
    texbuf->info.components = TEXTURE_COMPONENTS_RGB;
    texbuf->info.function = TEXTURE_FUNCTION_DECAL;

    clut.storage_mode = CLUT_STORAGE_MODE1;
    clut.start = 0;
    clut.psm = 0;
    clut.load_method = CLUT_NO_LOAD;
    clut.address = 0;

    q = draw_texture_sampling(q, 0, &lod);
    q = draw_texturebuffer(q, 0, texbuf, &clut);

    // Now send the packet, no need to wait since it's the first.
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_wait_fast();

    packet_free(packet);

}

int render(canvas *c)
{

    // Matrices to setup the 3D environment and camera
    MATRIX P;
    MATRIX MV;
    MATRIX CAM;
    MATRIX FINAL;

    prim_t prim;

    geometry g;
    g.vertex_count = vertex_count;
    g.vertices = vertices;
    g.colors = colors;
    g.coordinates = coordinates;
    g.index_count = points_count;
    g.indices = points;


    // Define the triangle primitive we want to use.
//    prim.type = PRIM_TRIANGLE;
//    prim.shading = PRIM_SHADE_GOURAUD;
//    prim.mapping = DRAW_DISABLE;
//    prim.fogging = DRAW_DISABLE;
//    prim.blending = DRAW_DISABLE;
//    prim.antialiasing = DRAW_ENABLE;
//    prim.mapping_type = PRIM_MAP_ST;
//    prim.colorfix = PRIM_UNFIXED;

    prim.type = PRIM_TRIANGLE;
    prim.shading = PRIM_SHADE_GOURAUD;
    prim.mapping = DRAW_ENABLE;
    prim.fogging = DRAW_DISABLE;
    prim.blending = DRAW_ENABLE;
    prim.antialiasing = DRAW_DISABLE;
    prim.mapping_type = PRIM_MAP_ST;
    prim.colorfix = PRIM_UNFIXED;

    frustum(P, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

    wait();

    int i=0, j=0;

    VECTOR scale;
    float size = 43.5f;
    scale[0] = size;
    scale[1] = size/1.33f;
    scale[2] = size;

    // The main loop...
    for (;;)
    {

        create_wand(c);

        clear(c);

        create_CAM(CAM, camera_position, camera_rotation);

//        object_rotation[0] += 0.008f;
//        object_rotation[1] += 0.012f;
//        object_rotation[2] += 0.012f;

//        for (i = -1; i <= 1; i++)
        {
//            for (j = -1; j <= 1; j++)
            {
                object_position[1] = 15.000f * j;
                object_position[0] = 15.000f * i;
                matrix_unit(MV);
                matrix_scale(MV, MV, scale);
                matrix_rotate(MV, MV, object_rotation);
                matrix_translate(MV, MV, object_position);
                create_FINAL(FINAL, MV, CAM, P);
                drawObject(c, FINAL, &g, &prim);
            }
        }

        use_wand(c);

        c->current_buffer ^= 1;

        draw_wait_finish();
        graph_wait_vsync();

    }

    // ~~ Free if we're done with the for loop, but we never will be
//    packet_free(packets[0]);
//    packet_free(packets[1]);

    return 0;

}

void startup(int width, int height)
{

    init_dma();

    //TODO: can this be the one to produce the canvas?
    canvas c;
    create_canvas(&c, width, height);
    clear_color(&c, 0x80, 0, 0x80);



    texbuffer_t texbuf;
    texbuf.width = 256;
    texbuf.psm = GS_PSM_24;
    texbuf.address = graph_vram_allocate(256,256,GS_PSM_24,GRAPH_ALIGN_BLOCK);

    // Load the texture into vram.
    load_texture(&texbuf);

    // Setup texture buffer
    setup_texture(&texbuf);




    // Render the cube
    render(&c);

    // Sleep
    SleepThread();

}
