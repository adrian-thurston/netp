#ifndef __ATTRIBUTE_H
#define __ATTRIBUTE_H
struct ring;

static ssize_t ring_foo_show(
		struct ring *obj, char *buf );

static ssize_t ring_foo_store(
		struct ring *obj, const char *buf, size_t count );
struct ring;

static ssize_t ring_bar_show(
		struct ring *obj, char *buf );

static ssize_t ring_bar_store(
		struct ring *obj, const char *buf, size_t count );
struct device;

static ssize_t device_tracks_show(
		struct device *obj, char *buf );

static ssize_t device_tracks_store(
		struct device *obj, const char *buf, size_t count );
#endif
