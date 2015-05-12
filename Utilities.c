typedef struct {
    framebuffer_t frame;
    zbuffer_t z;
    color_t clear_color;
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

void clear_color(canvas *c, int r, int g, int b)
{
    color_t *clear_color = &c->clear_color;
    clear_color->r = r;
    clear_color->g = g;
    clear_color->b = b;
}

typedef struct {
    packet_t *packet;
    qword_t *dmatag;
    qword_t *q;
} wand;

void clear(wand *w, canvas *c)
{
    qword_t *q = w->q;

    framebuffer_t *frame = &c->frame;
    zbuffer_t *z = &c->z;
    color_t *cc = &c->clear_color;

    q = draw_disable_tests(q, 0, z);
    q = draw_clear(q, 0, 2048.0f-frame->width/2, 2048.0f-frame->height/2, frame->width, frame->height, cc->r, cc->g, cc->b);
    q = draw_enable_tests(q, 0, z);

    w->q = q;
}

void create_wand(wand *w, packet_t *packet)
{
    w->packet = packet;
    w->dmatag = packet->data;
    w->q = w->dmatag;
    w->q++;
}

void use_wand(wand *w)
{
    qword_t *q = w->q;
    qword_t *dmatag = w->dmatag;
    packet_t *packet = w->packet;

    DMATAG_END(dmatag, (q - packet->data)-1, 0, 0, 0);
    dma_wait_fast();
    dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
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
    VECTOR *temp_vertices;
    xyz_t *verts;
    color_t *colors;
} memory;

void drawObject(wand *w, memory *m, MATRIX FINAL, int vertex_count, VECTOR *vertices, VECTOR *v_colors, int index_count, int *indices, prim_t *prim, color_t *color)
{
    int i;
    VECTOR *temp_vertices = m->temp_vertices;
    xyz_t *verts = m->verts;
    color_t *colors = m->colors;

    calculate_vertices(temp_vertices, vertex_count, vertices, FINAL);

    draw_convert_xyz(verts, 2048, 2048, 32, vertex_count, (vertex_f_t*)temp_vertices);
    draw_convert_rgbq(colors, vertex_count, (vertex_f_t*)temp_vertices, (color_f_t*)v_colors, 0x80);



    w->q = draw_prim_start(w->q, 0, prim, color);
    //TODO: reuse i?
    for(i = 0; i < index_count; i++)
    {
        //TODO: probably this
        w->q->dw[0] = colors[indices[i]].rgbaq;
        w->q->dw[1] = verts[indices[i]].xyz;
        w->q++;
    }
    w->q = draw_prim_end(w->q, 2, DRAW_RGBAQ_REGLIST);

    w->q = draw_finish(w->q);
}
