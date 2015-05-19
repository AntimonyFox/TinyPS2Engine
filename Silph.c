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


// Pad libraries
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <math.h>

#include <libpad.h>



#include "bg.c"
extern unsigned char bg[];
#include "flower.c"
extern unsigned char flower[];
#include "player_0_0.c"
extern unsigned char player_0_0[];




// pad_dma_buf is provided by the user, one buf for each pad
// contains the pad's current state
static char padBuf[256] __attribute__((aligned(64)));

static char actAlign[6];
static int actuators;

VECTOR camera_position = { 0.00f, 0.00f, 100.00f, 1.00f };
VECTOR camera_rotation = { 0.00f, 0.00f,   0.00f, 1.00f };

/*
 * Local functions
 */

/*
 * loadModules()
 */
void loadModules()
{
    int ret;

    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    if (ret < 0) {
        SleepThread();
    }

    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
    if (ret < 0) {
        SleepThread();
    }
}

int waitPadReady(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
            printf("Please wait, pad(%d,%d) is in state %s\n",
                   port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    return 0;
}

int initializePad(int port, int slot)
{

    int ret;
    int modes;
    int i;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
    printf("The device has %d modes\n", modes);

    if (modes > 0) {
        printf("( ");
        for (i = 0; i < modes; i++) {
            printf("%d ", padInfoMode(port, slot, PAD_MODETABLE, i));
        }
        printf(")");
    }

    printf("It is currently using mode %d\n",
           padInfoMode(port, slot, PAD_MODECURID, 0));

    // If modes == 0, this is not a Dual shock controller
    // (it has no actuator engines)
    if (modes == 0) {
        printf("This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
        printf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
        printf("This is no Dual Shock controller??\n");
        return 1;
    }

    printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
    printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    waitPadReady(port, slot);
    printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
    printf("# of actuators: %d\n",actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
        printf("padSetActAlign: %d\n",
               padSetActAlign(port, slot, actAlign));
    }
    else {
        printf("Did not find any actuators.\n");
    }

    waitPadReady(port, slot);

    return 1;
}



void init_dma()
{
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
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
    int ret;
    int port, slot;
    struct padButtonStatus buttons;
    u32 paddata;
    u32 old_pad = 0;
    u32 new_pad;

    SifInitRpc(0);
    loadModules();
    padInit(0);

    port = 0; // 0 -> Connector 1, 1 -> Connector 2
    slot = 0; // Always zero if not using multitap

    if((ret = padPortOpen(port, slot, padBuf)) == 0) {
        printf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }

    if(!initializePad(port, slot)) {
        printf("pad initalization failed!\n");
        SleepThread();
    }




    // The main loop...
    for (;;)
    {

        // Check on pad
        ret=padGetState(port, slot);
        while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1)) {
            if(ret==PAD_STATE_DISCONN) {
                printf("Pad(%d, %d) is disconnected\n", port, slot);
            }
            ret=padGetState(port, slot);
        }
        ret = padRead(port, slot, &buttons); // port, slot, buttons


        // Begin drawing
        create_wand(c);

        // Clear screen
        clear(c);

        // Update camera
        create_CAM(*CAM, camera_position, camera_rotation);



        // Draw objects
        drawObject(c, &e_bg);

        if (ret != 0) {
            paddata = 0xffff ^ buttons.btns;

            new_pad = paddata & ~old_pad;
            old_pad = paddata;

//            if(buttons.rjoy_h > 0xf0)
            float rX = buttons.rjoy_h / 127.0f - 1;
            float rY = -(buttons.rjoy_v / 127.0f - 1);
            if (hypot(rX, rY) > 0.4f)
                e_player_0_0.angle = (float)atan2(rY, rX);

            // Directions
            if (new_pad & PAD_LEFT)
                e_player_0_0.angle = (float)atan2(0, -1);
            if (new_pad & PAD_RIGHT)
                e_player_0_0.angle = (float)atan2(0, 1);
            if (new_pad & PAD_UP)
                e_player_0_0.angle = (float)atan2(1, 0);
            if (new_pad & PAD_DOWN)
                e_player_0_0.angle = (float)atan2(-1, 0);
        }
        drawObject(c, &e_player_0_0);



        // End drawing
        use_wand(c);

        c->current_buffer ^= 1;

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
