#include <dma.h>
#include <dma_tags.h>
#include <draw.h>
#include <draw3d.h>
#include <gif_tags.h>
#include <graph.h>
#include <gs_psm.h>
#include <kernel.h>
#include <libpad.h>
#include <loadfile.h>
#include <math.h>
#include <math3d.h>
#include <packet.h>
#include <sifrpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <tamtypes.h>
#include <time.h>

//#include <debug.h>


#include "square_data.c"

typedef struct {
    VECTOR *temp_vertices;
    xyz_t *verts;
    color_t *colors;
    texel_t *coordinates;
    color_t color;
    MATRIX P;
    MATRIX MV;
    MATRIX CAM;
    MATRIX FINAL;
} memory;

typedef struct {
    packet_t *packet;
    qword_t *q;
} wand;

typedef struct {
    int vertex_count;
    VECTOR *vertices;
    VECTOR *colors;
    VECTOR *coordinates;
    int index_count;
    int *indices;
} geometry;

typedef struct {
    framebuffer_t frame;
    zbuffer_t z;
    color_t clear_color;
    prim_t prim;
    geometry sprite_geometry;
    packet_t *buffers[2];
    int current_buffer;
    wand wand;
    //this has to be last and no one knows why
    memory memory;
} canvas;

void init_dma()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

packet_t * create_packet(int size)
{
    return packet_init(size, PACKET_NORMAL);
}

void delete_packet(packet_t *packet)
{
    packet_free(packet);
}

void send_packet(packet_t *packet, qword_t *q)
{
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
}

void wait()
{
    dma_wait_fast();
}

void register_canvas(canvas *c)
{
    packet_t *packet = create_packet(16);
    qword_t *q = packet->data;

    q = draw_setup_environment(q, 0, &c->frame, &c->z);
    q = draw_primitive_xyoffset(q, 0, (2048-c->frame.width/2), (2048-c->frame.height/2));
    q = draw_finish(q);

    send_packet(packet, q);
    wait();

    delete_packet(packet);
}

void * make_buffer(int element_size, int vertex_count)
{
    return memalign(128, element_size * vertex_count);
}

void set_frustum(canvas *c, float aspect, float left, float right, float bottom, float top, float near, float far)
{
    create_view_screen(c->memory.P, aspect, left, right, bottom, top, near, far);
}

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

    // Create perspective
    float s = 1.0f / 15.65f;
    s *= 640.0f / 2.0f;
    set_frustum(&c, 1.00f, -s, s, -s, s, 1.00f, 2000.00f);

    return c;

}

void ready_canvas(canvas *c)
{
    wand *w = &c->wand;
    packet_t *packet = c->buffers[c->current_buffer];

    w->packet = packet;
    w->q = packet->data;
    w->q++;
}

void commit_canvas(canvas *c)
{
    wand *w = &c->wand;

    qword_t *q = w->q;
    packet_t *packet = w->packet;

    q = draw_finish(q);
    wait ();
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

    // Sync processors
    draw_wait_finish();
    graph_wait_vsync();

    // Toggle buffers
    c->current_buffer ^= 1;
}

void set_clear_color(canvas *c, int r, int g, int b)
{
    color_t *clear_color = &c->clear_color;
    clear_color->r = r;
    clear_color->g = g;
    clear_color->b = b;
}

void clear(canvas *c)
{
    wand *w = &c->wand;
    qword_t *q = w->q;

    framebuffer_t *frame = &c->frame;
    zbuffer_t *z = &c->z;
    color_t *cc = &c->clear_color;

    q = draw_disable_tests(q, 0, z);
    q = draw_clear(q, 0, 2048.0f-frame->width/2, 2048.0f-frame->height/2, frame->width, frame->height, cc->r, cc->g, cc->b);
    q = draw_enable_tests(q, 0, z);

    w->q = q;
}

void create_MV(MATRIX MV, VECTOR translation, VECTOR rotation)
{
    create_local_world(MV, translation, rotation);
}

void create_CAM(MATRIX CAM, VECTOR translation, VECTOR rotation)
{
    create_world_view(CAM, translation, rotation);
}

void create_FINAL(MATRIX FINAL, MATRIX MV, MATRIX CAM, MATRIX P)
{
    create_local_screen(FINAL, MV, CAM, P);
}

typedef struct {
    clutbuffer_t clut;
    lod_t lod;
    texbuffer_t buffer;
    float left;
    float right;
    float top;
    float bottom;
    VECTOR size;
} sprite;

typedef struct {
    sprite *sprite;
    VECTOR position;
    VECTOR scale;
    float angle;
    VECTOR size;
} entity;

void set_size(entity *e, float width, float height)
{
    e->size[0] = width;
    e->size[1] = height;
}

void set_width(entity *e, float width)
{
    float aspect = e->sprite->size[0] / e->sprite->size[1];
    e->size[0] = width;
    e->size[1] = width / aspect;
}

void set_height(entity *e, float height)
{
    float aspect = e->sprite->size[0] / e->sprite->size[1];
    e->size[0] = height * aspect;
    e->size[1] = height;
}

sprite load_sprite(char *texture, int raw_width, int raw_height, int sprite_width, int sprite_height, int left_offset, int top_offset)
{

    sprite s;

    texbuffer_t *buffer = &s.buffer;
    buffer->width = raw_width;
    buffer->psm = GS_PSM_32;
    buffer->address = graph_vram_allocate(raw_width, raw_height, GS_PSM_32, GRAPH_ALIGN_BLOCK);



    // Crop
    s.top = top_offset / (float)raw_height;
    s.bottom = (top_offset + sprite_height) / (float)raw_height;

    s.left = left_offset / (float)raw_width;
    s.right = (left_offset + sprite_width) / (float)raw_width;

    // Scale
    s.size[0] = sprite_width;
    s.size[1] = sprite_height;
    s.size[2] = 1;



    packet_t *packet = create_packet(50);

    qword_t *q = packet->data;

    q = packet->data;

    q = draw_texture_transfer(q, texture, raw_width, raw_height, GS_PSM_32, buffer->address, buffer->width);
    q = draw_texture_flush(q);

    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0,0);
    dma_wait_fast();

    packet_free(packet);



    // Using a texture involves setting up a lot of information.
    clutbuffer_t *clut = &s.clut;
    lod_t *lod = &s.lod;

    lod->calculation = LOD_USE_K;
    lod->max_level = 0;
    lod->mag_filter = LOD_MAG_NEAREST;
    lod->min_filter = LOD_MIN_NEAREST;
    lod->l = 0;
    lod->k = 0;

    clut->storage_mode = CLUT_STORAGE_MODE1;
    clut->start = 0;
    clut->psm = 0;
    clut->load_method = CLUT_NO_LOAD;
    clut->address = 0;

    buffer->info.width = draw_log2(raw_width);
    buffer->info.height = draw_log2(raw_height);
    buffer->info.components = TEXTURE_COMPONENTS_RGBA;
    buffer->info.function = TEXTURE_FUNCTION_DECAL;

    return s;

}

void use_sprite(canvas *c, sprite *s)
{

    // Tell renderer to use this sprite's texture
    wand *w = &c->wand;
    qword_t *q = w->q;
    q = draw_texture_sampling(q, 0, &s->lod);
    q = draw_texturebuffer(q, 0, &s->buffer, &s->clut);

    // Modify the sprite square data to cut out the right amount from the top and bottom to eliminate black space
    geometry *g = &c->sprite_geometry;
    g->coordinates[0][0] = g->coordinates[2][0] = s->left;
    g->coordinates[1][0] = g->coordinates[3][0] = s->right;

    g->coordinates[0][1] = g->coordinates[1][1] = s->bottom;
    g->coordinates[2][1] = g->coordinates[3][1] = s->top;

    w->q = q;

}

void drawObject(canvas *c, entity *e)
{
    int i;
    u64 *dw;
    prim_t *prim = &c->prim;
    geometry *g = &c->sprite_geometry;
    wand *w = &c->wand;
    memory *m = &c->memory;

    VECTOR *temp_vertices = m->temp_vertices;
    xyz_t *verts = m->verts;
    color_t *colors = m->colors;
    texel_t *coordinates = m->coordinates;
    color_t *color = &m->color;



    // Texture the square with this entity's texture
    use_sprite(c, e->sprite);



    // Apply transformations
    MATRIX *P = &c->memory.P;
    MATRIX *MV = &c->memory.MV;
    MATRIX *CAM = &c->memory.CAM;
    MATRIX *FINAL = &c->memory.FINAL;

    matrix_unit(*MV);
    matrix_scale(*MV, *MV, e->size);
    VECTOR rotation = { 0, 0, e->angle };
    matrix_rotate(*MV, *MV, rotation);
    matrix_translate(*MV, *MV, e->position);
    create_FINAL(*FINAL, *MV, *CAM, *P);



    calculate_vertices(temp_vertices, g->vertex_count, g->vertices, *FINAL);

    draw_convert_xyz(verts, 2048, 2048, 32, g->vertex_count, (vertex_f_t*)temp_vertices);
    draw_convert_rgbq(colors, g->vertex_count, (vertex_f_t*)temp_vertices, (color_f_t*)g->colors, 0x80);
    draw_convert_st(coordinates, g->vertex_count, (vertex_f_t*)temp_vertices, (texel_f_t*)g->coordinates);


    dw = (u64*)draw_prim_start(w->q, 0, prim, color);
    for(i = 0; i < g->index_count; i++)
    {
        *dw++ = colors[g->indices[i]].rgbaq;
        *dw++ = coordinates[g->indices[i]].uv;
        *dw++ = verts[g->indices[i]].xyz;
    }

    // Check if we're in middle of a qword or not.
    if ((u32)dw % 16)
        *dw++ = 0;

    // Only 3 registers rgbaq/st/xyz were used (standard STQ reglist)
    w->q = draw_prim_end((qword_t*)dw, 3, DRAW_STQ_REGLIST);
}

entity create_entity(sprite *s)
{
    entity e;
    e.sprite = s;
    e.scale[0] = e.scale[1] = 1;
    e.angle = 0;
    e.size[0] = s->size[0];
    e.size[1] = s->size[1];
    e.size[2] = s->size[2];
    return e;
}

void loadPadModules()
{
    SifInitRpc(0);

    int ret;
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0)
        SleepThread();  //aka "die"

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0)
        SleepThread();

    padInit(0);
}

typedef char __attribute__((aligned(64))) pad_buffer[256];
typedef struct {
    int port;
    int slot;
    int hasBeenDisconnected;
    int numActuators;
    char actAlign[6];
    pad_buffer *padBuf;
    struct padButtonStatus buttons;
    u32 paddata;
    u32 old_pad;
    u32 new_pad;
    clock_t smallMotorStart;
    clock_t bigMotorStart;
    float smallMotorDur;
    float bigMotorDur;
} pad;

int pad_still_connected(pad *pad)
{
    int state = padGetState(pad->port, pad->slot);
    int success = (state == PAD_STATE_STABLE);
    pad->hasBeenDisconnected |= !success;
    return success;
}

void wait_pad_ready(pad *pad)
{
    int port = pad->port;
    int slot = pad->slot;

    int state;
    do {
        state = padGetState(port, slot);
    } while ( (state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1) );
}

int initialize_pad(pad *pad)
{
    int port = pad->port;
    int slot = pad->slot;



    wait_pad_ready(pad);

    int numModes = padInfoMode(port, slot, PAD_MODETABLE, -1);

    if (numModes == 0)
        SleepThread();

    int i;
    for (i = 0; i < numModes; i++) {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
    };
    if (i >= numModes)
        return 0;

    int ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0)
        return 0;

    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    wait_pad_ready(pad);
    padInfoPressMode(port, slot);

    wait_pad_ready(pad);
    padEnterPressMode(port, slot);

    wait_pad_ready(pad);
    pad->numActuators = padInfoAct(port, slot, -1, 0);

    if (pad->numActuators != 0) {
        pad->actAlign[0] = 0;   // Enable small engine
        pad->actAlign[1] = 1;   // Enable big engine
        pad->actAlign[2] = 0xff;
        pad->actAlign[3] = 0xff;
        pad->actAlign[4] = 0xff;
        pad->actAlign[5] = 0xff;

        wait_pad_ready(pad);
        padSetActAlign(port, slot, pad->actAlign);
    }

    wait_pad_ready(pad);

    return 0;
}

pad create_pad(int port, int slot)
{

    pad pad;
    pad.port = port;
    pad.slot = slot;
    pad.old_pad = 0;
    pad.padBuf = memalign(64, 256);
    pad.smallMotorDur = -1;
    pad.bigMotorDur = -1;


    // Will break here if there's something wrong with padBuf
    int ret = padPortOpen(port, slot, pad.padBuf);
    if (ret == 0)
        SleepThread();


    initialize_pad(&pad);


    return pad;
}

void reinitialize_pad(pad *pad)
{
    initialize_pad(pad);
}

void set_small_motor(pad *pad, int value)
{
    pad->actAlign[0] = value;
    padSetActDirect(pad->port, pad->slot, pad->actAlign);
}

void start_small_motor(pad *pad)
{
    set_small_motor(pad, 1);
}

void stop_small_motor(pad *pad)
{
    set_small_motor(pad, 0);
}

void run_small_motor(pad *pad, float seconds)
{
    set_small_motor(pad, 1);
    pad->smallMotorStart = clock();
    pad->smallMotorDur = seconds;
}

void set_big_motor(pad *pad, int value)
{
    pad->actAlign[1] = value;
    padSetActDirect(pad->port, pad->slot, pad->actAlign);
}

void start_big_motor(pad *pad)
{
    set_big_motor(pad, 255);
}

void stop_big_motor(pad *pad)
{
    set_big_motor(pad, 0);
}

void run_big_motor(pad *pad, int strength, float seconds)
{
    set_big_motor(pad, strength);
    pad->bigMotorStart = clock();
    pad->bigMotorDur = seconds;
}

int update_pad(pad *pad)
{
    if (!pad_still_connected(pad))
        return 0;

    if (pad->hasBeenDisconnected) {
        pad->hasBeenDisconnected = 0;
        reinitialize_pad(pad);
    }

    struct padButtonStatus *buttons = &pad->buttons;
    int ret = padRead(pad->port, pad->slot, buttons);
    if (ret) {
        pad->paddata = 0xffff ^ buttons->btns;
        pad->new_pad = pad->paddata & ~pad->old_pad;
        pad->old_pad = pad->paddata;
    }

    // Rumble
    if (pad->smallMotorDur != -1) {
        if ((float)(clock() - pad->smallMotorStart) / CLOCKS_PER_SEC >= pad->smallMotorDur) {
            pad->smallMotorDur = -1;
            stop_small_motor(pad);
        }
    }
    if (pad->bigMotorDur != -1) {
        if ((float)(clock() - pad->bigMotorStart) / CLOCKS_PER_SEC >= pad->bigMotorDur) {
            pad->bigMotorDur = -1;
            stop_big_motor(pad);
        }
    }

    return ret;
}
