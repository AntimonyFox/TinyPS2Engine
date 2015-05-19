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
    //TODO: close_packet?
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

void create_wand(canvas *c)
{
    wand *w = &c->wand;
    packet_t *packet = c->buffers[c->current_buffer];

    w->packet = packet;
    w->q = packet->data;
    w->q++;
}

void use_wand(canvas *c)
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
    MATRIX SCALE;
} sprite;

typedef struct {
    sprite *sprite;
    VECTOR position;
    VECTOR scale;
    float angle;
} entity;

sprite load_sprite(char *texture, int raw_width, int raw_height, int sprite_width, int sprite_height, int left_offset, int top_offset)
{

    sprite s;

    texbuffer_t *buffer = &s.buffer;
    buffer->width = raw_width;
    buffer->psm = GS_PSM_32;
    buffer->address = graph_vram_allocate(raw_width, raw_height, GS_PSM_32, GRAPH_ALIGN_BLOCK);



    s.top = top_offset / (float)raw_height;
    s.bottom = (top_offset + sprite_height) / (float)raw_height;

    s.left = left_offset / (float)raw_width;
    s.right = (left_offset + sprite_width) / (float)raw_width;

    float max = (raw_width > raw_height) ? raw_width : raw_height;
    float default_width = sprite_width / max;
    float default_height = sprite_height / max;

    VECTOR scale = { default_width, default_height, 1 };
    MATRIX *SCALE = &s.SCALE;
    matrix_unit(*SCALE);
    matrix_scale(*SCALE, *SCALE, scale);



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
    matrix_multiply(*MV, *MV, e->sprite->SCALE);
    matrix_scale(*MV, *MV, e->scale);
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
    return e;
}

static char padBuf[256] __attribute__((aligned(64)));
static char actAlign[6];
static int numActuators;
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

typedef struct {
    int port;
    int slot;
} pad;

int waitPadReady(pad *pad)
{
    int port = pad->port;
    int slot = pad->slot;

    int state = padGetState(port, slot);
    while ( (state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1) ) {
        state=padGetState(port, slot);
    }
    return 0;
}

int initializePad(pad *pad, void *padBuf)
{

    int port = pad->port;
    int slot = pad->slot;



    int ret = padPortOpen(port, slot, padBuf);
    if (ret == 0)
        return 0;



    waitPadReady(pad);

    int numModes = padInfoMode(port, slot, PAD_MODETABLE, -1);

    if (numModes == 0)
        return 1;

    int i;
    for (i = 0; i < numModes; i++) {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
    };
    if (i >= numModes)
        return 1;

    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0)
        return 1;

    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(pad);
    padInfoPressMode(port, slot);

    waitPadReady(pad);
    padEnterPressMode(port, slot);

    waitPadReady(pad);
    numActuators = padInfoAct(port, slot, -1, 0);

    if (numActuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(pad);
        padSetActAlign(port, slot, actAlign);
    }

    waitPadReady(pad);

    return 1;
}
