//
// Created by jiashu on 18-10-27.
//

#include <stdio.h>
#include <stdint.h>
#include "log_csv.h"

/**
 * save date to csv file
 * @param title this's filename and csv title.
 * @param ylable
 * @param line_name
 * @param times
 * @param len
 */
void log_csv(const char *title, const char *ylable, const char *line_name, const double *times, size_t len)
{
    char file[128];
    sprintf(file, "%s_%s.csv", title, line_name);
    FILE* fd = fopen(file, "w+");

    fprintf(fd, "title, %s\n", title);
    fprintf(fd, "ylabel, %s\n", ylable);
    fprintf(fd, "line_label, %s\n", line_name);

    for (uint32_t i = 0; i < len; ++i) {
        fprintf(fd, "%12.3f\n", times[i]);
    }

    fclose(fd);
}
