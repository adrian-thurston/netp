#ifndef __ATTRIBUTE_H
#define __ATTRIBUTE_H

struct filter;
struct device;

static ssize_t filter_foo_show(
		struct filter *obj, char *buf );

static ssize_t filter_foo_store(
		struct filter *obj, const char *buf, size_t count );

static ssize_t filter_bar_show(
		struct filter *obj, char *buf );

static ssize_t filter_bar_store(
		struct filter *obj, const char *buf, size_t count );

static ssize_t device_tracks_show(
		struct device *obj, char *buf );

static ssize_t device_tracks_store(
		struct device *obj, const char *buf, size_t count );

#endif
