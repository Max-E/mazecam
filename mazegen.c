/*
Mazedemo, by Max Eliaser

Copyright (c) 2014 Intel Corp.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "mazedemo_common.h"

typedef unsigned char byte;


typedef struct
{
    int width, height, area;
    int num_wall_list;
    byte *mark_cells, *mark_passages, *in_wall_list;
    int *wall_list;
} maze_t;

static inline bool get_bitmask (const byte *mask, int entry)
{
    return (mask[entry>>3] & (1<<(entry&7))) != 0;
}

static inline void clear_bitmask (byte *mask, int entry)
{
    mask[entry>>3] &= ~(1<<(entry&7));
}

static inline void set_bitmask (byte *mask, int entry)
{
    mask[entry>>3] |= 1<<(entry&7);
}

static byte *make_bitmask (int size)
{
    byte *ret = malloc (size>>3);
    memset (ret, 0, size>>3);
    return ret;
}

static void initialize_maze (maze_t *out, int width, int height)
{
    out->width = width;
    out->height = height;
    out->area = width * height;
    
    int npassages = 2 * out->area;
    
    out->mark_cells = make_bitmask (out->area);
    out->mark_passages = make_bitmask (npassages);
    out->in_wall_list = make_bitmask (npassages);
    
    out->num_wall_list = 0;
    out->wall_list = malloc (sizeof(int) * npassages);
}

static int wall_num_between_cells (const maze_t *maze, int cell1_num, int cell2_num)
{
    if (cell2_num < cell1_num)
        return wall_num_between_cells (maze, cell2_num, cell1_num);

    int ret = 2 * cell1_num;    
    if (cell2_num == cell1_num + 1 && cell2_num % maze->width != 0)
        return ret;
    else if (cell2_num == cell1_num + maze->width)
        return ret + 1;
    else 
        assert (false);
}

static int add_wall_to_list (maze_t *maze, int wall_num, int avoid_wall_num)
{
    if (wall_num != avoid_wall_num && !get_bitmask (maze->in_wall_list, wall_num))
    {
        assert (!get_bitmask (maze->mark_passages, wall_num));
        set_bitmask (maze->in_wall_list, wall_num);
        maze->wall_list[maze->num_wall_list++] = wall_num;
    }
}

static int visit_cell (maze_t *maze, int cell_num, int from_wall)
{
    set_bitmask (maze->mark_cells, cell_num);
    if (cell_num % maze->width)
        add_wall_to_list (maze, wall_num_between_cells (maze, cell_num, cell_num-1), from_wall);
    if ((cell_num + 1) % maze->width)
        add_wall_to_list (maze, wall_num_between_cells (maze, cell_num, cell_num+1), from_wall);
    if (cell_num >= maze->width)
        add_wall_to_list (maze, wall_num_between_cells (maze, cell_num, cell_num-maze->width), from_wall);
    if ((cell_num + maze->width) < maze->area)
        add_wall_to_list (maze, wall_num_between_cells (maze, cell_num, cell_num+maze->width), from_wall);
}

static void handle_wall (maze_t *maze, int wall_list_num)
{
    int wall_num;
    int cell1_num, cell2_num;
    
    wall_num = maze->wall_list[wall_list_num];
    
    cell1_num = wall_num/2;
    
    if (wall_num % 2)
        cell2_num = cell1_num + maze->width;
    else
        cell2_num = cell1_num + 1;
    
    bool cell1_visited = get_bitmask (maze->mark_cells, cell1_num);
    bool cell2_visited = get_bitmask (maze->mark_cells, cell2_num);
    
    if (cell1_visited && !cell2_visited)
    {
        set_bitmask (maze->mark_passages, wall_num);
        visit_cell (maze, cell2_num, wall_num);
    }
    else if (!cell1_visited && cell2_visited)
    {
        set_bitmask (maze->mark_passages, wall_num);
        visit_cell (maze, cell1_num, wall_num);
    }
    else if (cell1_visited && cell2_visited)
    {
        clear_bitmask (maze->in_wall_list, wall_num);
        maze->num_wall_list--;
        if (wall_list_num != maze->num_wall_list)
            maze->wall_list[wall_list_num] = maze->wall_list[maze->num_wall_list];
    }
    else
    {
        assert (false);
    }
}

static void generate_maze_lines (const maze_t *maze, mazepublic_t *out)
{
    int row, col, cell;
    
    size_t lines_size = sizeof(*out->lines) * (maze->width + 1) * (maze->height + 1) + 4;
    out->lines = malloc (lines_size);
    memset (out->lines, 0, lines_size);

    out->numlines = 0;
    
    #define outline (out->lines[out->numlines])
    
    #define ADD_VERTLINE(x,y1,y2) \
    { \
        outline[0][0] = outline[1][0] = x; \
        outline[0][1] = y1; \
        outline[1][1] = y2; \
        out->numlines++; \
    }
    
    #define ADD_HORIZLINE(y,x1,x2) \
    { \
        outline[0][1] = outline[1][1] = y; \
        outline[0][0] = x1; \
        outline[1][0] = x2; \
        out->numlines++; \
    }
    
    ADD_HORIZLINE (0, 2, 2*maze->width);
    ADD_VERTLINE (0, 0, 2*maze->width);
    ADD_HORIZLINE (2*maze->height, 0, 2*maze->width - 2);
    
    for (row = 0, cell = 0; row < maze->height; row++)
    {
        for (col = 0; col < maze->width; col++, cell++)
        {
            if (!get_bitmask (maze->mark_passages, 2*cell+1) && row < maze->height - 1)
                ADD_HORIZLINE (2*row + 2, 2*col, 2*col + 2);
            if (!get_bitmask (maze->mark_passages, 2*cell))
                ADD_VERTLINE (2*col + 2, 2*row, 2*row + 2);
        }
    }
}

maze_t maze;

void generate_maze (int width, int height, mazepublic_t *out)
{
    srand (time (NULL));
    
    initialize_maze (&maze, width, height);
    
    visit_cell (&maze, rand () % maze.area, -1);
    
    while (maze.num_wall_list)
    {
        int wall_list_num = rand () % maze.num_wall_list;
        handle_wall (&maze, wall_list_num);
    }

    generate_maze_lines (&maze, out);
}

// arrows must be of size num_arrows
// Returns true if the maze is solved by the directions given
bool maze_trace (int num_arrows, arrow_t *arrows, mazepublic_t *out)
{
    mazepoint_t lastpoint = {0, 0};
    
    size_t lines_size = sizeof(*out->lines) * (num_arrows + 1);
    out->lines = malloc (lines_size);
    memset (out->lines, 0, lines_size);
    
    out->numlines = 0;
    
    for (int i = 0; i < num_arrows; i++)
    {
        mazepoint_t nextpoint;
        memcpy (nextpoint, lastpoint, sizeof(lastpoint));
        switch (arrows[i].dir)
        {
            case arrow_up:
                nextpoint[1]--;
                break;
            case arrow_down:
                nextpoint[1]++;
                break;
            case arrow_left:
                nextpoint[0]--;
                break;
            case arrow_right:
                nextpoint[0]++;
                break;
        }
        
        if (nextpoint[0] < 0 || nextpoint[0] >= maze.width)
            continue;
        if (nextpoint[1] < 0 || nextpoint[1] >= maze.height)
            continue;
        
        int last_cellnum = lastpoint[1] * maze.width + lastpoint[0];
        int next_cellnum = nextpoint[1] * maze.width + nextpoint[0];
        
        if (get_bitmask (maze.mark_passages, wall_num_between_cells (&maze, last_cellnum, next_cellnum)))
        {
            for (int j = 0; j < 2; j++)
            {
                outline[0][j] = 2 * lastpoint[j] + 1;
                outline[1][j] = 2 * nextpoint[j] + 1;
            }
            out->numlines++;
            memcpy (lastpoint, nextpoint, sizeof(lastpoint));
        }
    }
    
    return lastpoint[0] == maze.width - 1 && lastpoint[1] == maze.height - 1;
}
