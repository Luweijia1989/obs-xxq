#ifndef BE_RENDER_DEFINE_H
#define BE_RENDER_DEFINE_H

typedef struct be_rgba_color
{
    float red;
    float green;
    float blue;
    float alpha;
}be_rgba_color;

typedef struct be_render_helper_line
{
    float x1;
    float y1;
    float x2;
    float y2;
}be_render_helper_line;

#endif
