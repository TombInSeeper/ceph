//
// Created by jiashu on 18-10-27.
//

#ifndef OCSSD_BAIDU_LOG_CSV_H
#define OCSSD_BAIDU_LOG_CSV_H

#include <stddef.h>

/**
 * save date to csv file
 * @param title this's filename and csv title.
 * @param ylable
 * @param line_name
 * @param times
 * @param len
 */
void log_csv(const char *title, const char *ylable, const char *line_name, const double *times, size_t len);

#endif //OCSSD_BAIDU_LOG_CSV_H
