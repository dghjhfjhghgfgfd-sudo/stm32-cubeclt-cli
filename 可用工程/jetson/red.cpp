#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <queue>
#include <string>
#include <vector>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct Buffer { void *start; size_t length; };
struct Target { bool found; int area; int x; int y; int w; int h; int cx; int cy; };

static int xioctl(int fd, unsigned long request, void *arg) {
    int r;
    do { r = ioctl(fd, request, arg); } while (r == -1 && errno == EINTR);
    return r;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static unsigned char clamp255(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

static void yuv_to_rgb(int y, int u, int v, unsigned char &r, unsigned char &g, unsigned char &b) {
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;
    r = clamp255((298 * c + 409 * e + 128) >> 8);
    g = clamp255((298 * c - 100 * d - 208 * e + 128) >> 8);
    b = clamp255((298 * c + 516 * d + 128) >> 8);
}

static bool is_red_rgb(int r, int g, int b) {
    // Tune here if needed. This is intentionally simple and visible.
    return r > 130 && g < 120 && b < 120 && r > g * 15 / 10 && r > b * 15 / 10 && (r - std::max(g, b)) > 45;
}

static void yuyv_to_bgra_and_mask(const unsigned char *src, unsigned char *dst, std::vector<unsigned char> &mask, int width, int height, int scale) {
    int sw = width / scale;
    int sh = height / scale;
    std::fill(mask.begin(), mask.end(), 0);

    for (int y = 0; y < height; ++y) {
        const unsigned char *row = src + (size_t)y * (size_t)width * 2U;
        unsigned char *out = dst + (size_t)y * (size_t)width * 4U;
        for (int x = 0; x < width; x += 2) {
            int y0 = row[x * 2];
            int u = row[x * 2 + 1];
            int y1 = row[x * 2 + 2];
            int v = row[x * 2 + 3];
            unsigned char r, g, b;
            yuv_to_rgb(y0, u, v, r, g, b);
            out[x * 4 + 0] = b; out[x * 4 + 1] = g; out[x * 4 + 2] = r; out[x * 4 + 3] = 0;
            if ((x % scale == 0) && (y % scale == 0) && is_red_rgb(r, g, b)) mask[(y / scale) * sw + (x / scale)] = 1;

            yuv_to_rgb(y1, u, v, r, g, b);
            out[(x + 1) * 4 + 0] = b; out[(x + 1) * 4 + 1] = g; out[(x + 1) * 4 + 2] = r; out[(x + 1) * 4 + 3] = 0;
            if (((x + 1) % scale == 0) && (y % scale == 0) && is_red_rgb(r, g, b)) mask[(y / scale) * sw + ((x + 1) / scale)] = 1;
        }
    }
}

static Target find_largest_blob(const std::vector<unsigned char> &mask, int width, int height, int scale) {
    int sw = width / scale;
    int sh = height / scale;
    std::vector<unsigned char> seen(sw * sh, 0);
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    int best_area = 0, best_minx = 0, best_miny = 0, best_maxx = 0, best_maxy = 0, best_sumx = 0, best_sumy = 0;

    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            int idx = y * sw + x;
            if (!mask[idx] || seen[idx]) continue;
            std::queue<int> q;
            q.push(idx); seen[idx] = 1;
            int area = 0, minx = x, maxx = x, miny = y, maxy = y, sumx = 0, sumy = 0;
            while (!q.empty()) {
                int cur = q.front(); q.pop();
                int cy = cur / sw;
                int cx = cur % sw;
                area++; sumx += cx; sumy += cy;
                minx = std::min(minx, cx); maxx = std::max(maxx, cx);
                miny = std::min(miny, cy); maxy = std::max(maxy, cy);
                for (int k = 0; k < 4; ++k) {
                    int nx = cx + dx[k], ny = cy + dy[k];
                    if (nx < 0 || nx >= sw || ny < 0 || ny >= sh) continue;
                    int ni = ny * sw + nx;
                    if (mask[ni] && !seen[ni]) { seen[ni] = 1; q.push(ni); }
                }
            }
            if (area > best_area) {
                best_area = area;
                best_minx = minx; best_miny = miny; best_maxx = maxx; best_maxy = maxy;
                best_sumx = sumx; best_sumy = sumy;
            }
        }
    }

    if (best_area < 35) return {false, best_area, 0, 0, 0, 0, -1, -1};
    int cx = (best_sumx / best_area) * scale;
    int cy = (best_sumy / best_area) * scale;
    return {true, best_area, best_minx * scale, best_miny * scale,
            (best_maxx - best_minx + 1) * scale, (best_maxy - best_miny + 1) * scale, cx, cy};
}

static void draw_rect(unsigned char *img, int width, int height, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(width - 1, x + w), y1 = std::min(height - 1, y + h);
    for (int t = 0; t < 3; ++t) {
        int yt = std::min(height - 1, y0 + t), yb = std::max(0, y1 - t);
        for (int xx = x0; xx <= x1; ++xx) {
            unsigned char *p1 = img + ((size_t)yt * width + xx) * 4U;
            unsigned char *p2 = img + ((size_t)yb * width + xx) * 4U;
            p1[0] = 0; p1[1] = 255; p1[2] = 0;
            p2[0] = 0; p2[1] = 255; p2[2] = 0;
        }
        int xl = std::min(width - 1, x0 + t), xr = std::max(0, x1 - t);
        for (int yy = y0; yy <= y1; ++yy) {
            unsigned char *p1 = img + ((size_t)yy * width + xl) * 4U;
            unsigned char *p2 = img + ((size_t)yy * width + xr) * 4U;
            p1[0] = 0; p1[1] = 255; p1[2] = 0;
            p2[0] = 0; p2[1] = 255; p2[2] = 0;
        }
    }
}

static void draw_cross(unsigned char *img, int width, int height, int cx, int cy) {
    if (cx < 0 || cy < 0) return;
    for (int d = -10; d <= 10; ++d) {
        int x = cx + d, y = cy;
        if (x >= 0 && x < width && y >= 0 && y < height) {
            unsigned char *p = img + ((size_t)y * width + x) * 4U; p[0] = 255; p[1] = 0; p[2] = 0;
        }
        x = cx; y = cy + d;
        if (x >= 0 && x < width && y >= 0 && y < height) {
            unsigned char *p = img + ((size_t)y * width + x) * 4U; p[0] = 255; p[1] = 0; p[2] = 0;
        }
    }
}

int main(int argc, char **argv) {
    const char *video_path = argc > 1 ? argv[1] : "/dev/video1";
    int vfd = open(video_path, O_RDWR | O_NONBLOCK, 0);
    if (vfd < 0) { perror("open video"); return 1; }

    struct v4l2_format fmt;
    CLEAR(fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (xioctl(vfd, VIDIOC_S_FMT, &fmt) < 0) { perror("VIDIOC_S_FMT"); return 1; }
    int width = (int)fmt.fmt.pix.width;
    int height = (int)fmt.fmt.pix.height;

    struct v4l2_streamparm parm;
    CLEAR(parm);
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = 30;
    xioctl(vfd, VIDIOC_S_PARM, &parm);

    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(vfd, VIDIOC_REQBUFS, &req) < 0) { perror("VIDIOC_REQBUFS"); return 1; }

    std::vector<Buffer> buffers(req.count);
    for (unsigned int i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(vfd, VIDIOC_QUERYBUF, &buf) < 0) { perror("VIDIOC_QUERYBUF"); return 1; }
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, vfd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) { perror("mmap"); return 1; }
        if (xioctl(vfd, VIDIOC_QBUF, &buf) < 0) { perror("VIDIOC_QBUF"); return 1; }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(vfd, VIDIOC_STREAMON, &type) < 0) { perror("VIDIOC_STREAMON"); return 1; }

    Display *display = XOpenDisplay(NULL);
    Window win = 0;
    GC gc = 0;
    XImage *ximg = NULL;
    std::vector<unsigned char> rgb(width * height * 4U);
    const int scale = 4;
    std::vector<unsigned char> mask((width / scale) * (height / scale));

    if (display) {
        int screen = DefaultScreen(display);
        win = XCreateSimpleWindow(display, RootWindow(display, screen), 60, 60, width, height, 1,
                                  BlackPixel(display, screen), WhitePixel(display, screen));
        XStoreName(display, win, "jetson手势识别测试");
        XSelectInput(display, win, ExposureMask | KeyPressMask | StructureNotifyMask);
        XMapWindow(display, win);
        gc = XCreateGC(display, win, 0, NULL);
        ximg = XCreateImage(display, DefaultVisual(display, screen), DefaultDepth(display, screen),
                            ZPixmap, 0, (char*)rgb.data(), width, height, 32, 0);
    }

    printf("red target tracker started video=%s format=%dx%d YUYV display=%s\n", video_path, width, height, display ? "on" : "off");
    fflush(stdout);

    int frame_count = 0;
    double fps_start = now_sec();
    double last_print = 0.0;
    bool running = true;
    Target last_target = {false, 0, 0, 0, 0, 0, -1, -1};
    int lost_frames = 99;

    while (running) {
        struct pollfd pfd;
        pfd.fd = vfd;
        pfd.events = POLLIN;
        int pr = poll(&pfd, 1, 2000);
        if (pr <= 0) { fprintf(stderr, "poll timeout/error\n"); continue; }

        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (xioctl(vfd, VIDIOC_DQBUF, &buf) < 0) {
            if (errno == EAGAIN) continue;
            perror("VIDIOC_DQBUF"); break;
        }

        const unsigned char *frame = (const unsigned char *)buffers[buf.index].start;
        yuyv_to_bgra_and_mask(frame, rgb.data(), mask, width, height, scale);
        Target raw_target = find_largest_blob(mask, width, height, scale);
        Target target = raw_target;
        if (raw_target.found) {
            last_target = raw_target;
            lost_frames = 0;
        } else if (lost_frames < 3 && last_target.found) {
            target = last_target;
            target.found = true;
            lost_frames++;
        } else {
            lost_frames++;
        }

        double t = now_sec();
        frame_count++;
        if (t - last_print >= 0.2) {
            if (target.found) {
                printf("target=red x=%d y=%d area=%d box=%d,%d,%d,%d\n", target.cx, target.cy, target.area, target.x, target.y, target.w, target.h);
            } else {
                printf("target=none area=%d\n", target.area);
            }
            fflush(stdout);
            last_print = t;
        }
        if (frame_count % 30 == 0) {
            double fps = 30.0 / (t - fps_start);
            printf("fps=%.2f\n", fps);
            fflush(stdout);
            fps_start = t;
        }

        if (display && ximg) {
            if (target.found) {
                draw_rect(rgb.data(), width, height, target.x, target.y, target.w, target.h);
                draw_cross(rgb.data(), width, height, target.cx, target.cy);
            }
            XPutImage(display, win, gc, ximg, 0, 0, 0, 0, width, height);
            std::string label = target.found
                ? "red target x=" + std::to_string(target.cx) + " y=" + std::to_string(target.cy) + " area=" + std::to_string(target.area)
                : "red target: none";
            XSetForeground(display, gc, 0x00ff00);
            XDrawString(display, win, gc, 12, 24, label.c_str(), (int)label.size());
            XFlush(display);
            while (XPending(display)) {
                XEvent e;
                XNextEvent(display, &e);
                if (e.type == KeyPress || e.type == DestroyNotify) running = false;
            }
        }

        if (xioctl(vfd, VIDIOC_QBUF, &buf) < 0) { perror("VIDIOC_QBUF requeue"); break; }
    }

    if (ximg) { ximg->data = NULL; XDestroyImage(ximg); }
    if (display) { XFreeGC(display, gc); XDestroyWindow(display, win); XCloseDisplay(display); }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(vfd, VIDIOC_STREAMOFF, &type);
    for (auto &b : buffers) munmap(b.start, b.length);
    close(vfd);
    return 0;
}
