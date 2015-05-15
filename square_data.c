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

 int points_count = 6;

// ~~ Indices
int points[6] = {
    0,  1,  2,
    1,  2,  3
};

int vertex_count = 4;

VECTOR vertices[4] = {
    {  -10.00f,  -10.00f,  0.00f, 1.00f },
    {  10.00f,  -10.00f,  0.00f, 1.00f },
    {  -10.00f,  10.00f,  0.00f, 1.00f },
    {  10.00f,  10.00f,  0.00f, 1.00f }
};

VECTOR colors[4] = {
//    { 1.00f, 0.00f, 0.00f, 1.00f },
//    { 0.00f, 1.00f, 0.00f, 1.00f },
//    { 0.00f, 0.00f, 1.00f, 1.00f },
//    { 1.00f, 0.00f, 1.00f, 1.00f }

    { 1.00f, 1.00f, 1.00f, 1.00f },
    { 1.00f, 1.00f, 1.00f, 1.00f },
    { 1.00f, 1.00f, 1.00f, 1.00f },
    { 1.00f, 1.00f, 1.00f, 1.00f }

};

VECTOR coordinates[4] = {
    { 0.00f, 0.00f, 0.00f, 0.00f },
    { 1.00f, 0.00f, 0.00f, 0.00f },
    { 0.00f, 1.00f, 0.00f, 0.00f },
    { 1.00f, 1.00f, 0.00f, 0.00f }
};
