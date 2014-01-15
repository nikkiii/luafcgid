#include "main.h"

/* utility functions */

request_t* luaL_checkrequest(lua_State* L, int i) {
    request_t* r = NULL;
    luaL_checkudata(L, i, "LuaFCGId.Request");
    r = (request_t*)lua_unboxpointer(L, i);
    return r;
}

void luaL_loadrequest(lua_State* L) {
    luaL_newmetatable(L, "LuaFCGId.Request");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_register(L, NULL, request_methods);
    lua_pop(L, 1);
}

void luaL_pushrequest(lua_State* L, request_t* r) {
    lua_boxpointer(L, r);
    luaL_getmetatable(L, "LuaFCGId.Request");
    lua_setmetatable(L, -2);
}

/* request methods */
const struct luaL_Reg request_methods[] = {
    {"status", L_req_status},
    {"header", L_req_header},
    {"cookie", L_req_cookie},
    {"gets", L_req_gets},
    {"puts", L_req_puts},
    {"flush", L_req_flush},
    {"reset", L_req_reset},
    {"config", L_req_config},
    {"log", L_req_log},
    {NULL, NULL}
};

/* r:status(code) */
int L_req_status(lua_State *L) {
    request_t* r = NULL;
    int status_code;
    char buf[8];
    if (lua_gettop(L) >= 2) {
        r = luaL_checkrequest(L, 1);
        status_code = luaL_checknumber(L, 2);
        
        snprintf(buf, 8, "%d", status_code);
        
        strcpy(r->httpstatus, buf);
    }
    return 0;
}

/* r:header(string, <string>) */
int L_req_header(lua_State *L) {
    request_t* r = NULL;
    const char* s1 = NULL;
    const char* s2 = NULL;
    size_t l1, l2 = 0;
    if (lua_gettop(L) >= 2) {
        r = luaL_checkrequest(L, 1);
        s1 = luaL_checklstring(L, 2, &l1);
        
        if(strcmp(s1, "Location") == 0) {
            strcpy(r->httpstatus, "302 Found");
        } else if(strcmp(s1, "Content-Type") == 0) {
			strcpy(r->contenttype, s2);
			return 0;
		}
        
        if (lua_gettop(L) == 3) {
            s2 = luaL_checklstring(L, 3, &l2);
            buffer_add(&r->header, s1, l1);
            buffer_add(&r->header, ": ", -1);
            buffer_add(&r->header, s2, l2);
            buffer_add(&r->header, CRLF, -1);
        } else {
            buffer_add(&r->header, s1, l1);
            buffer_add(&r->header, CRLF, -1);
        }
    }
    return 0;
}

/* r:cookie(key, value, expire, path, domain) */
int L_req_cookie(lua_State *L) {
    request_t* r = NULL;
    const char* s1 = NULL;
    const char* s2 = NULL;
    const char* s3 = NULL;
    const char* s4 = NULL;
    size_t l1, l2, l3, l4 = 0;
    if (lua_gettop(L) >= 3) {
        r = luaL_checkrequest(L, 1);
        s1 = luaL_checklstring(L, 2, &l1);
        s2 = luaL_checklstring(L, 3, &l2);
        
        buffer_add(&r->header, "Set-Cookie: ", -1);
        buffer_add(&r->header, s1, l1);
        buffer_add(&r->header, "=", -1);
        buffer_add(&r->header, s2, l2);
        
        if (lua_gettop(L) >= 4) {
            time_t t = luaL_checknumber(L, 4);
            char buffer[30];
            struct tm *my_tm = gmtime(&t);
            strftime(buffer, RFC1123_TIME_LEN + 1, "%a, %d %b %Y %H:%M:%S GMT", my_tm);
            buffer_add(&r->header, "; Expires=", -1);
            buffer_add(&r->header, buffer, RFC1123_TIME_LEN);
        }
        if (lua_gettop(L) >= 5) {
            s4 = luaL_checklstring(L, 5, &l3);
            buffer_add(&r->header, "; Path=", -1);
            buffer_add(&r->header, s3, l3);
        }
        if (lua_gettop(L) >= 6) {
            s4 = luaL_checklstring(L, 6, &l4);
            buffer_add(&r->header, "; Domain=", -1);
            buffer_add(&r->header, s4, l4);
        }
        buffer_add(&r->header, CRLF, -1);
    }
    return 0;
}

/* r:puts(string) */
int L_req_puts(lua_State *L) {
    request_t* r = NULL;
    const char* s = NULL;
    size_t l = 0;
    if (lua_gettop(L) >= 2) {
        r = luaL_checkrequest(L, 1);
        s = luaL_checklstring(L, 2, &l);
        if (r->buffering) {
            /* add to output buffer */
            buffer_add(&r->body, s, l);
        } else {
            /* make sure headers are sent before any data */
            if(!r->headers_sent)
                send_header(r);
            FCGX_PutStr(s, l, r->fcgi.out);
        }
    }
    return 0;
}

/* r:flush() */
int L_req_flush(lua_State *L) {
    request_t* r = NULL;
    if (lua_gettop(L)) {
        r = luaL_checkrequest(L, 1);
           send_body(r);
        r->body.len = 0;
    }
    return 0;
}

/* r:reset() */
int L_req_reset(lua_State *L) {
    request_t* r = NULL;
    if (lua_gettop(L)) {
        r = luaL_checkrequest(L, 1);
        r->body.len = 0;
    }
    return 0;
}

/* r:gets() returns string */
int L_req_gets(lua_State *L) {
    request_t* r = NULL;
    if (lua_gettop(L)) {
        r = luaL_checkrequest(L, 1);
        luaL_pushcgicontent(L, r);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* r:config(string, <string>) returns string */
int L_req_config(lua_State *L) {
    request_t* r = NULL;
    const char* n = NULL;
    const char* v = NULL;
    size_t ln, lv = 0;
    if (lua_gettop(L) >= 2) {
        r = luaL_checkrequest(L, 1);
        n = luaL_checklstring(L, 2, &ln);
        if (lua_gettop(L) == 3) {
            v = luaL_checklstring(L, 3, &lv);
            /* TODO: set config value */
        } else {
            /* TODO: get config value */
            lua_pushnil(L);
            return 1;
        }
    }
    return 0;
}

/* r:log(string) */
int L_req_log(lua_State *L) {
    request_t* r = NULL;
    const char* s = NULL;
    size_t l = 0;
    if (lua_gettop(L) >= 2) {
        r = luaL_checkrequest(L, 1);
        s = luaL_checklstring(L, 2, &l);
        logit(s);
    }
    return 0;
}
