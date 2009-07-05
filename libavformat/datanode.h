/*
 * Format to represent ini (and later xml) as doubly linked 2D data nodes
 * Copyright (c) 2009 Geza Kovacs
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVFORMAT_DATANODE_H
#define AVFORMAT_DATANODE_H

#include "avformat.h"

#define DATANODE_STR_BUFSIZE 5

typedef struct DataNode {
    char *name;
    char *value;
    struct DataNode *child;
    struct DataNode *parent;
    struct DataNode *next;
    struct DataNode *prev;
} DataNode;

typedef struct StringList {
    char *str;
    struct StringList *next;
} StringList;

DataNode *ff_datanode_mkchild(DataNode *o);

DataNode *ff_datanode_mknext(DataNode *o);

DataNode *ff_datanode_tree_from_ini(char *p);

DataNode *ff_datanode_tree_from_xml(char *p);

DataNode *ff_datanode_getlognext(DataNode *d);

void ff_datanode_filter_values_by_name(DataNode *d, StringList *l, char *n);

int ff_datanode_getdepth(DataNode *d);

void ff_datanode_visualize(DataNode *d);

StringList *ff_stringlist_alloc();

void ff_stringlist_append(StringList *l, char *str);

void ff_stringlist_print(StringList *l);

#endif /* AVFORMAT_DATANODE_H */
