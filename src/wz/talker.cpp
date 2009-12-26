
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <auto/config.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <wz/daemon.hpp>

#if defined(MODEL_FREEBSD_KQUEUE)
#include <sys/event.h>
#elif defined(MODEL_LINUX_EPOLL)
#include <sys/epoll.h>
#else
#error "Unknown model!"
#endif

#define WZ_BUFFER_SIZE 8192

std::map<int, wz_talker> wz_talkers;

wz_talker::wz_talker() : cur_header(0), fd(-1), state(R_UNDEFINED), response(0), request(0)
{
    memset(&ip, 0, sizeof(struct in_addr));
    last_access = time(0);
    buffer_cur_read = buffer_cur_write = buffer = 0;
}

void wz_talker::init(int client_fd, const struct in_addr &addr)
{
    if (!buffer) buffer = new char[WZ_BUFFER_SIZE];
    memset(buffer, 0, WZ_BUFFER_SIZE);
    buffer_cur_read = buffer_cur_write = buffer;

    if (!request) request = new wz_request;
    memset(request, 0, sizeof(wz_request));
    if (!response) response = new wz_response;
    memset(response, 0, sizeof(wz_response));

    request->ip = addr;

    fd = client_fd;
    ip = addr;
    state = R_READ_REQUEST_LINE;
    handle();
}

void wz_talker::reset(int dont_close)
{
    if (request->post_body != 0)
    {
        free(request->post_body);
        request->post_body = 0;
        request->content_len = 0;
    }
    if (-1 == fd) return;
    if (dont_close != 1)
    {
#if defined(MODEL_FREEBSD_KQUEUE)
        watchEvent(fd, EVFILT_READ, EV_DELETE, 0, 0);
#elif defined(MODEL_LINUX_EPOLL)
        watchEvent(fd, 0, EPOLL_CTL_DEL);
#else
#error "Unknown model!"
#endif
        close(fd);
        fd = -1;
        memset(&ip, 0, sizeof(struct in_addr));
    }

    memset(request, 0, sizeof(wz_request));
    memset(response, 0, sizeof(wz_response));
    memset(buffer, 0, WZ_BUFFER_SIZE);
    buffer_cur_read = buffer_cur_write = buffer;
    request->ip = ip;
}

void wz_talker::done()
{
    reset();
    if (buffer) delete[] buffer;
    buffer = 0;
    if (request) delete request;
    request = 0;
    if (response) delete response;
    response = 0;
}

bool wz_talker::is_timeout(const time_t &t)
{
    if (fd != -1) return t - last_access > 30;
    return false;
}

void wz_talker::handle()
{
    if (-1 == fd)
    {
        wz_log_debug(WZ_ERR, "signal received by arleady closed socket");
        return;
    }
    last_access = time(0);
    cur_header = 0;
    int in_loop = 1;
    while (in_loop)
    {
        switch (state)
        {
            case R_READ_REQUEST_LINE:
            {
                char *req_line = get_line();
                if (req_line)
                {
                    do_request_line(req_line);
                } else in_loop = 0;
                break;
            }
            case R_READ_HEADER:
            {
                char *req_line = get_line();
                if (req_line)
                {
                    do_header_line(req_line);
                    ++cur_header;
                } else in_loop = 0;
                if (state != R_READ_POST_BODY) break;
            }
            case R_READ_POST_BODY:
            {
                if (request->content_len <= 0)
                {
                    state = R_HANDLE;
                    break;
                }
                if (request->post_body == 0)
                {
                    request->post_body = (char*)calloc(request->content_len, 1);
                    readed_bytes = 0;
                }
                if (request->post_body != 0)
                {
                    int have_to_read_1 = strlen(buffer_cur_read);

                    if (have_to_read_1)
                    {
                        int max_to_read_1 = request->content_len - readed_bytes;
                        max_to_read_1 = have_to_read_1 > max_to_read_1 ? max_to_read_1 : have_to_read_1;
                        strncpy(request->post_body + readed_bytes, buffer_cur_read, max_to_read_1);
                        readed_bytes += max_to_read_1;
                        buffer_cur_read += have_to_read_1;
                    }

                    int max_to_read = request->content_len - readed_bytes;
                    int r = read(fd, request->post_body + readed_bytes, max_to_read);
                    if (r == -1)
                    {
                        if (errno != EWOULDBLOCK)
                        {
                            wz_log_error(WZ_ERR, "read error for socket %d, closing: %s", fd, strerror(errno));
                            reset();
                            in_loop = 0;
                        }
                        else
                        {
                            in_loop = 0;
                        }
                    } else {
                        readed_bytes += r;
                        if (readed_bytes >= request->content_len)
                        {
                            state = R_HANDLE;
                        }
                        else
                        {
                            if (r == 0) in_loop = 0;
                        }
                    }
                } else state = R_CLOSE;
                break;
            }
            case R_HANDLE:
            {
                do_handle();
                break;
            }
            case R_REPLY:
            {
                do_reply();
                if (request->keep_alive)
                {
                    reset(1);
                    state = R_READ_REQUEST_LINE;
                } else state = R_CLOSE;
                break;
            }
            case R_CLOSE:
            {
                reset();
                in_loop = 0;
                break;
            }
            default:
            {
                wz_log_debug(WZ_ERR, "default action on state %d for %d, closing", state, fd);
                reset();
                in_loop = 0;
            }
        }
    }
}

char *wz_talker::get_line()
{
    // paranoic, nevertheless we must be sure we have eol in buffer
    buffer_cur_write[0] = 0;
    char *result = 0;
    char *nl = strchr(buffer_cur_read, '\n');
    if (nl)
    {
        result = buffer_cur_read;
        buffer_cur_read = nl + 1;
        nl[0] = 0;
        nl--;
        if (nl[0] == '\r') nl[0] = 0;
    } else {
        int max_to_read = WZ_BUFFER_SIZE - (buffer_cur_write - buffer) - 1;
        if (!max_to_read)
        {
            wz_log_warn(WZ_ERR, "out of buffer memory for fd=%d, ip=%s", fd, inet_ntoa(ip));
            reset();
            return 0;
        }

        int r = read(fd, buffer_cur_write, max_to_read);
        if (r == -1)
        {
            if (errno != EWOULDBLOCK)
            {
                wz_log_debug(WZ_ERR, "read error for socket %d, closing: %s", fd, strerror(errno));
                reset();
            }
        } else {
            if (!r) return 0;
            buffer_cur_write += r;
            return get_line();
        }
    }
    return result;
}

void strtolower(char *str)
{
    for (; *str; ++str)
    {
        *str = tolower(*str);
    }
}

int parse_request(char *line, wz_request *r)
{
    char *method;
    char *uri;
    char *uri_params;
    char *proto;
    char *major;
    char *minor;

    method = line;

    if (!(uri = strchr(method, ' '))) return 400;
    *uri++ = 0;
    while ((*uri==' ') && (*uri)) uri++;

    if (!*uri) return 400;

    if (!(proto = strchr(uri, ' '))) return 400;

    *proto++ = 0;
    while ((*proto==' ') && (*proto)) proto++;
    if (!*proto) return 400;

    if (!(major = strchr(proto, '/'))) return 400;
    *major++ = 0;

    if (!(minor = strchr(major, '.'))) return 400;
    *minor++ = 0;

    strtolower(method);
    if (strcmp(method, "get") == 0)
        r->method = WZ_METHOD_GET;
    else if (strcmp(method, "head") == 0)
        r->method = WZ_METHOD_HEAD;
    else if (strcmp(method, "post") == 0)
        r->method = WZ_METHOD_POST;
    else return 501;

    strtolower(proto);
    if (strcmp(proto, "http")) return 400;

    r->major = atol(major);
    if (r->major < 1) r->major = 1;
    r->minor = atol(minor);
    if (r->minor < 0) r->minor = 0;

    if(r->major > 1) return 505;

    uri_params = strchr(uri, '?');
    if (uri_params)
    {
        uri_params[0] = 0;
        uri_params++;
        r->uri_params = uri_params;
    }

    r->uri_path = uri;

    return 0;
}

int parse_header_line(char *line, wz_request *r, size_t cur_header)
{
    char* value;
    
    value = strchr(line, ':');
    if (!value) return 400;
    value++[0] = 0;
    
    while ((*value==' ') && (*value)) value++;
    if (!*value) return 0;

    strtolower(line);

    if (strlen(value) > 2048) value[2048] = 0;

    if (cur_header < WZ_HEADER_MAX)
    {
        r->headers[cur_header].key = line;
        r->headers[cur_header].value = value;
    }

    if (!strcmp(line, "cookie"))
    {
        r->cookie = value;
        return 0;
    }

    if (!strcmp(line, "user-agent"))
    {
        strtolower(value);
        r->user_agent = value;
        return 0;
    }

    if (!strncmp(line, "content-len", 10))
    {
        r->content_len = atoi(value);
        return 0;
    }

    if (!strcmp(line, "connection"))
    {
        strtolower(value);
        if (r->minor && !strncmp(value, "keep-alive", 10)) r->keep_alive = 1;
        return 0;
    }

    if (!strcmp(line, "x-forwarded-for"))
    {
        r->x_forwarded_for = value;
        return 0;
    }

    if (!strcmp(line, "referer"))
    {
        r->referer = value;
        return 0;
    }

    if (!strcmp(line, "host"))
    {
        r->host = value;
        return 0;
    }

    return 0;
}

void wz_talker::do_request_line(char *s)
{
    state = R_READ_HEADER;

    if (0 != (response->status = parse_request(s, request)))
    {
        response->content_type = "text/plain";
        response->body_len = sizeof("4xx\n") - 1;
        response->body = response->status < 500 ? "4xx\n" : "5xx\n";
        state = R_REPLY;
        return;
    }

    if (!cfg->r.access_log_file_name.empty())
    {
        wz_log_access(WZ_ACC, "%s|%s?%s|", inet_ntoa(request->ip),
            request->uri_path, request->uri_params);
    }
}

void wz_talker::do_header_line(char *s)
{
    if (0 == *s)
    {
        if (request->method == WZ_METHOD_POST && request->content_len > 0)
        {
            state = R_READ_POST_BODY;
        }
        else
        {
            state = R_HANDLE;
        }
    }
    else
    {
        if (0 != (response->status = parse_header_line(s, request, cur_header)))
        {
            response->content_type = "text/plain";
            response->body_len = sizeof("4xx\n") - 1;
            response->body = "4xx\n";
            state = R_REPLY;
        }
    }
}

void wz_talker::do_reply()
{
    int l, i;
    char b[WZ_BUFFER_SIZE];
    char now_str[128];
    time_t now_time;
    time(&now_time);
    memset(now_str, 0, 128);
    strftime(now_str, 127, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now_time));

    l = snprintf(b, WZ_BUFFER_SIZE, 
        "HTTP/%d.%d %d %s\r\n"
        "Server: muflon/" WZ_HTTPD_VERSION "\r\n"
        "Date: %s\r\n",
        request->major, request->minor, response->status,
        "", now_str);


    if (response->content_type)
    {
        l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Content-Type: %s\r\n",
            response->content_type);
    }

    l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Connection: %s\r\n",
        request->keep_alive ? "keep-alive" : "close");

    if (response->location)
    {
        l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Location: %s\r\n",
            response->location);
    }

    if (response->set_cookie || response->set_cookie2)
    {
        l += snprintf(b + l, WZ_BUFFER_SIZE - l, "P3P: policyref=\"/w3c/p3p.xml\", CP=\"NON DSP "
            "COR CUR ADM DEV PSA PSD OUR UNR BUS UNI COM NAV INT DEM STA\"\r\n");

        if (response->set_cookie)
        {
            l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Set-Cookie: %s\r\n",
                response->set_cookie);
        }

        if (response->set_cookie2)
        {
            l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Set-Cookie: %s\r\n",
                response->set_cookie2);
        }
    }

    if (!response->can_cache)
    {
        l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Pragma: no-cache\r\n"
                                                 "Cache-control: no-cache\r\n");
    }

    l += snprintf(b + l, WZ_BUFFER_SIZE - l, "Accept-Ranges: bytes\r\nContent-Length: %d\r\n",
        (response->body && response->body_len) ? response->body_len : 0);

    for (i = 0; i < response->headers_size; ++i)
    {
        l += snprintf(b + l, WZ_BUFFER_SIZE - l, "%s: %s\r\n",
            response->headers[i].key, response->headers[i].value);
    }

    l += snprintf(b + l, WZ_BUFFER_SIZE - l, "\r\n");

    if (0 > write(fd, b, l))
    {
        /* void */
    }
    
    if (response->body && response->body_len)
    {
        if (0 > write(fd, response->body, response->body_len))
        {
            /* void */
        }
    }

    state = R_CLOSE;
}

void wz_talker::do_handle()
{
    if (-1 == fd) return;
    if (response->status) return;

    worker.work(request, response);
    
    if (0 == response->status)
    {
        response->status = 404;
        response->content_type = "text/plain";
        response->body_len = sizeof("404\n") - 1;
        response->body = "404\n";
    }

    state = R_REPLY;
}

