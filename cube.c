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

#include "mesh_data.c"

VECTOR object_position = { 0.00f, 0.00f, 0.00f, 1.00f };
VECTOR object_rotation = { 0.00f, 0.00f, 0.00f, 1.00f };

VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };

void init_gs(framebuffer_t *frame, zbuffer_t *z)
{

	// Define a 32-bit 640x512 framebuffer.
//	frame->width = 640;
//	frame->height = 512;
	frame->width = 640;
    frame->height = 360;
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

}

void init_drawing_environment(framebuffer_t *frame, zbuffer_t *z)
{

	packet_t *packet = packet_init(16, PACKET_NORMAL);

	// This is our generic qword pointer.
	// ~~ This not only has a reference to the packet, but also where we are in the packet as we write to it
	qword_t *q = packet->data;

	// This will setup a default drawing environment.
	q = draw_setup_environment(q, 0, frame, z);

	// Now reset the primitive origin to 2048-width/2,2048-height/2.
	//TODO: play around with this
	// ~~ So I believe 2048x2048 is our total canvas size, and we're just using the bottom-right of it.
	// ~~ The follow points to the origin, which has 320 pixels to the left and right of it, and 256 above and below
	q = draw_primitive_xyoffset(q, 0, (2048-frame->width/2), (2048-frame->height/2));
//	q = draw_primitive_xyoffset(q, 0, (2048-320), (2048-256));

	// Finish setting up the environment.
	// ~~ Signal that drawing is finished
	q = draw_finish(q);

	// Now send the packet, no need to wait since it's the first.
	// ~~ arg0 : I have a feeling we'll always send it to the DMA_CHANNEL_GIF channel
	// ~~ arg1 : Beginning of packet
	// ~~ arg2 : Number of bytes to write (?)
	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

	// ~~ Wait until channel is usable by coprocessor status
	dma_wait_fast();

	packet_free(packet);

}

int render(framebuffer_t *frame, zbuffer_t *z)
{

	int i;
	int context = 0;

	// Matrices to setup the 3D environment and camera
	MATRIX P;
	MATRIX MV;
	MATRIX CAM;
	MATRIX FINAL;

	VECTOR *temp_vertices;

    prim_t prim;
	color_t color;

	xyz_t   *verts;
	color_t *colors;

	// The data packets for double buffering dma sends.
	packet_t *packets[2];
	packet_t *current;
	qword_t *q;
	qword_t *dmatag;

	packets[0] = packet_init(100,PACKET_NORMAL);
	packets[1] = packet_init(100,PACKET_NORMAL);

	temp_vertices = memalign(128, sizeof(VECTOR) * vertex_count);
	verts  = memalign(128, sizeof(vertex_t) * vertex_count);
	colors = memalign(128, sizeof(color_t)  * vertex_count);

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

	create_view_screen(P, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

	dma_wait_fast();

	// The main loop...
	for (;;)
	{
	    current = packets[context];

		dmatag = current->data;
		q = dmatag;
		q++;

        //gl.clear
		q = draw_disable_tests(q, 0, z);
		q = draw_clear(q, 0, 2048.0f-frame->width/2, 2048.0f-frame->height/2, frame->width, frame->height, 0x00, 0x00, 0x00);
		q = draw_enable_tests(q, 0, z);

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




        dmatag = current->data;
        q = dmatag;
        q++;

        //Object transformations
        object_position[0] = 15.000f;
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




		context ^= 1;

		draw_wait_finish();
		graph_wait_vsync();

	}

    // ~~ Free if we're done with the for loop, but we never will be
	packet_free(packets[0]);
	packet_free(packets[1]);

	return 0;

}

int main(int argc, char **argv)
{

	// The buffers to be used.
	framebuffer_t frame;
	zbuffer_t z;

	// Init GIF dma channel.
	// ~~ This happens all the time no matter what.
	// ~~ DMA_CHANNEL_GIF = 0x02
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);

	// Init the GS, framebuffer, and zbuffer.
	// ~~ Canvas size
	init_gs(&frame, &z);

	// Init the drawing environment and framebuffer.
	// ~~ THIS happens all the time no matter what.
	// ~~ This just saves your settings basically
	init_drawing_environment(&frame, &z);

	// Render the cube
	render(&frame, &z);

	// Sleep
	SleepThread();

	// End program.
	return 0;

}
