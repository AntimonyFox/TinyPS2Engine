typedef struct {
    framebuffer_t frame;
    zbuffer_t z;
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

void wait()
{
    dma_wait_fast();
}
