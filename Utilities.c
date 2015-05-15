typedef struct {
    VECTOR *temp_vertices;
    xyz_t *verts;
    color_t *colors;
    texel_t *coordinates;
    color_t color;
} memory;

typedef struct {
    packet_t *packet;
    qword_t *dmatag;
    qword_t *q;
} wand;

typedef struct {
    framebuffer_t frame;
    zbuffer_t z;
    color_t clear_color;
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

void frustum(MATRIX P, float aspect, float left, float right, float bottom, float top, float near, float far)
{
    create_view_screen(P, aspect, left, right, bottom, top, near, far);
}

void create_wand(canvas *c)
{
    wand *w = &c->wand;
    packet_t *packet = c->buffers[c->current_buffer];

    w->packet = packet;
    w->dmatag = packet->data;
    w->q = w->dmatag;
    w->q++;
}

void use_wand(canvas *c)
{
    wand *w = &c->wand;

    qword_t *q = w->q;
    qword_t *dmatag = w->dmatag;
    packet_t *packet = w->packet;

    q = draw_finish(q);
//    DMATAG_END(dmatag, (q - packet->data)-1, 0, 0, 0);
    wait ();
//    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
    dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
}

void clear_color(canvas *c, int r, int g, int b)
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
    int vertex_count;
    VECTOR *vertices;
    VECTOR *colors;
    VECTOR *coordinates;
    int index_count;
    int *indices;
} geometry;

void drawObject(canvas *c, MATRIX FINAL, geometry *g, prim_t *prim)
{
    int i;
    u64 *dw;
    wand *w = &c->wand;
    memory *m = &c->memory;

    VECTOR *temp_vertices = m->temp_vertices;
    xyz_t *verts = m->verts;
    color_t *colors = m->colors;
    texel_t *coordinates = m->coordinates;
    color_t *color = &m->color;

    calculate_vertices(temp_vertices, g->vertex_count, g->vertices, FINAL);

    draw_convert_xyz(verts, 2048, 2048, 32, g->vertex_count, (vertex_f_t*)temp_vertices);
    draw_convert_rgbq(colors, g->vertex_count, (vertex_f_t*)temp_vertices, (color_f_t*)g->colors, 0x80);
    draw_convert_st(coordinates, g->vertex_count, (vertex_f_t*)temp_vertices, (texel_f_t*)g->coordinates);



    dw = (u64*)draw_prim_start(w->q, 0, prim, color);
//    w->q = draw_prim_start(w->q, 0, prim, color);
    for(i = 0; i < g->index_count; i++)
    {
//        //TODO: probably this
//        w->q->dw[0] = colors[g->indices[i]].rgbaq;
//        w->q->dw[1] = verts[g->indices[i]].xyz;
//        w->q++;

        *dw++ = colors[g->indices[i]].rgbaq;
        *dw++ = coordinates[g->indices[i]].uv;
        *dw++ = verts[g->indices[i]].xyz;
    }

    // Check if we're in middle of a qword or not.
    if ((u32)dw % 16)
    {

        *dw++ = 0;

    }

    // Only 3 registers rgbaq/st/xyz were used (standard STQ reglist)
    w->q = draw_prim_end((qword_t*)dw, 3, DRAW_STQ_REGLIST);
//    w->q = draw_prim_end(w->q, 2, DRAW_RGBAQ_REGLIST);
}
