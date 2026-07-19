/* Lightweight Agent - Minimal dependencies for cross-compilation
 * - No TLS (HTTP only for local LLM server)
 * - No interactive terminal (isocline)
 * - Minimal dependencies: just cJSON + standard C library
 * - Static compilation friendly
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "cJSON.h"

#ifndef DT_DIR
#define DT_DIR 4
#endif

#define MAX_STEPS 50
#define BUFFER_SIZE 65536
#define MAX_TOOLS 16

/* Simple HTTP client (no TLS) */
typedef struct {
    char *host;
    int port;
    char *api_key;
    char *model;
} HttpClient;

/* Tool context */
typedef struct {
    char *workspace;
} ToolContext;

/* Tool result */
typedef struct {
    char *output;
    int success;
} ToolResult;

/* Simple string buffer */
typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StrBuf;

static void sb_init(StrBuf *sb) {
    sb->data = malloc(1024);
    sb->len = 0;
    sb->capacity = 1024;
    sb->data[0] = '\0';
}

static void sb_append(StrBuf *sb, const char *str) {
    size_t str_len = strlen(str);
    while (sb->len + str_len + 1 > sb->capacity) {
        sb->capacity *= 2;
        sb->data = realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->len, str, str_len + 1);
    sb->len += str_len;
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    sb_append(sb, buf);
}

static char *sb_take(StrBuf *sb) {
    char *result = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
    return result;
}

static void sb_free(StrBuf *sb) {
    free(sb->data);
}

/* HTTP request to local server */
static char *http_post(HttpClient *client, const char *path, const char *body) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "Socket creation failed\n");
        return NULL;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(client->port);

    if (inet_pton(AF_INET, client->host, &server.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address\n");
        close(sock);
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        fprintf(stderr, "Connection failed to %s:%d\n", client->host, client->port);
        close(sock);
        return NULL;
    }

    /* Build HTTP request */
    StrBuf request;
    sb_init(&request);
    sb_appendf(&request, "POST %s HTTP/1.1\r\n", path);
    sb_appendf(&request, "Host: %s:%d\r\n", client->host, client->port);
    sb_append(&request, "Content-Type: application/json\r\n");
    if (client->api_key) {
        sb_appendf(&request, "Authorization: Bearer %s\r\n", client->api_key);
    }
    sb_appendf(&request, "Content-Length: %zu\r\n", strlen(body));
    sb_append(&request, "Connection: close\r\n");
    sb_append(&request, "\r\n");
    sb_append(&request, body);

    /* Send request */
    send(sock, request.data, request.len, 0);
    sb_free(&request);

    /* Read response */
    StrBuf response;
    sb_init(&response);
    char buffer[4096];
    ssize_t n;
    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        sb_append(&response, buffer);
    }
    close(sock);

    /* Extract body */
    char *body_start = strstr(response.data, "\r\n\r\n");
    if (!body_start) {
        sb_free(&response);
        return NULL;
    }
    body_start += 4;

    char *result = strdup(body_start);
    sb_free(&response);
    return result;
}

/* Tool implementations */

static ToolResult tool_read_file(ToolContext *ctx, const char *file_path) {
    ToolResult result = {0};

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", ctx->workspace, file_path);

    FILE *f = fopen(full_path, "r");
    if (!f) {
        StrBuf sb;
        sb_init(&sb);
        sb_appendf(&sb, "Error: Cannot open file %s", file_path);
        result.output = sb_take(&sb);
        result.success = 0;
        return result;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    size_t nread = fread(content, 1, size, f);
    content[nread] = '\0';
    fclose(f);

    result.output = content;
    result.success = 1;
    return result;
}

static ToolResult tool_write_file(ToolContext *ctx, const char *file_path, const char *content) {
    ToolResult result = {0};

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", ctx->workspace, file_path);

    FILE *f = fopen(full_path, "w");
    if (!f) {
        StrBuf sb;
        sb_init(&sb);
        sb_appendf(&sb, "Error: Cannot write to file %s", file_path);
        result.output = sb_take(&sb);
        result.success = 0;
        return result;
    }

    fwrite(content, 1, strlen(content), f);
    fclose(f);

    StrBuf sb;
    sb_init(&sb);
    sb_appendf(&sb, "Successfully wrote to %s", file_path);
    result.output = sb_take(&sb);
    result.success = 1;
    return result;
}

static ToolResult tool_list_files(ToolContext *ctx, const char *path) {
    ToolResult result = {0};

    char full_path[1024];
    if (path && strlen(path) > 0) {
        snprintf(full_path, sizeof(full_path), "%s/%s", ctx->workspace, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", ctx->workspace);
    }

    DIR *dir = opendir(full_path);
    if (!dir) {
        StrBuf sb;
        sb_init(&sb);
        sb_appendf(&sb, "Error: Cannot open directory %s", path ? path : ".");
        result.output = sb_take(&sb);
        result.success = 0;
        return result;
    }

    StrBuf sb;
    sb_init(&sb);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        sb_append(&sb, entry->d_name);
        if (entry->d_type == DT_DIR) {
            sb_append(&sb, "/");
        }
        sb_append(&sb, "\n");
    }
    closedir(dir);

    result.output = sb_take(&sb);
    result.success = 1;
    return result;
}

static ToolResult tool_bash(ToolContext *ctx, const char *command) {
    (void)ctx;
    ToolResult result = {0};

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        result.output = strdup("Error: Failed to execute command");
        result.success = 0;
        return result;
    }

    StrBuf sb;
    sb_init(&sb);
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        sb_append(&sb, buffer);
    }

    int status = pclose(pipe);
    result.output = sb_take(&sb);
    result.success = (status == 0);
    return result;
}

/* Execute a tool call */
static ToolResult execute_tool(ToolContext *ctx, const char *tool_name, cJSON *arguments) {
    printf("  [Tool] %s\n", tool_name);

    if (strcmp(tool_name, "read_file") == 0) {
        cJSON *path = cJSON_GetObjectItem(arguments, "file_path");
        if (path && cJSON_IsString(path)) {
            return tool_read_file(ctx, path->valuestring);
        }
    } else if (strcmp(tool_name, "write_file") == 0) {
        cJSON *path = cJSON_GetObjectItem(arguments, "file_path");
        cJSON *content = cJSON_GetObjectItem(arguments, "content");
        if (path && cJSON_IsString(path) && content && cJSON_IsString(content)) {
            return tool_write_file(ctx, path->valuestring, content->valuestring);
        }
    } else if (strcmp(tool_name, "list_files") == 0) {
        cJSON *path = cJSON_GetObjectItem(arguments, "path");
        const char *path_str = (path && cJSON_IsString(path)) ? path->valuestring : "";
        return tool_list_files(ctx, path_str);
    } else if (strcmp(tool_name, "bash") == 0) {
        cJSON *command = cJSON_GetObjectItem(arguments, "command");
        if (command && cJSON_IsString(command)) {
            return tool_bash(ctx, command->valuestring);
        }
    }

    ToolResult result = {0};
    result.output = strdup("Error: Unknown tool or invalid arguments");
    result.success = 0;
    return result;
}

/* Build chat completion request */
static char *build_request(cJSON *messages, const char *model) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", model);
    cJSON_AddItemToObject(root, "messages", cJSON_Duplicate(messages, 1));

    /* Add tools */
    cJSON *tools = cJSON_AddArrayToObject(root, "tools");

    /* read_file tool */
    cJSON *read_tool = cJSON_CreateObject();
    cJSON_AddStringToObject(read_tool, "type", "function");
    cJSON *read_func = cJSON_AddObjectToObject(read_tool, "function");
    cJSON_AddStringToObject(read_func, "name", "read_file");
    cJSON_AddStringToObject(read_func, "description", "Read contents of a file");
    cJSON *read_params = cJSON_AddObjectToObject(read_func, "parameters");
    cJSON_AddStringToObject(read_params, "type", "object");
    cJSON *read_props = cJSON_AddObjectToObject(read_params, "properties");
    cJSON *read_path = cJSON_AddObjectToObject(read_props, "file_path");
    cJSON_AddStringToObject(read_path, "type", "string");
    cJSON *read_req = cJSON_AddArrayToObject(read_params, "required");
    cJSON_AddItemToArray(read_req, cJSON_CreateString("file_path"));
    cJSON_AddItemToArray(tools, read_tool);

    /* write_file tool */
    cJSON *write_tool = cJSON_CreateObject();
    cJSON_AddStringToObject(write_tool, "type", "function");
    cJSON *write_func = cJSON_AddObjectToObject(write_tool, "function");
    cJSON_AddStringToObject(write_func, "name", "write_file");
    cJSON_AddStringToObject(write_func, "description", "Write content to a file");
    cJSON *write_params = cJSON_AddObjectToObject(write_func, "parameters");
    cJSON_AddStringToObject(write_params, "type", "object");
    cJSON *write_props = cJSON_AddObjectToObject(write_params, "properties");
    cJSON *write_path = cJSON_AddObjectToObject(write_props, "file_path");
    cJSON_AddStringToObject(write_path, "type", "string");
    cJSON *write_content = cJSON_AddObjectToObject(write_props, "content");
    cJSON_AddStringToObject(write_content, "type", "string");
    cJSON *write_req = cJSON_AddArrayToObject(write_params, "required");
    cJSON_AddItemToArray(write_req, cJSON_CreateString("file_path"));
    cJSON_AddItemToArray(write_req, cJSON_CreateString("content"));
    cJSON_AddItemToArray(tools, write_tool);

    /* list_files tool */
    cJSON *list_tool = cJSON_CreateObject();
    cJSON_AddStringToObject(list_tool, "type", "function");
    cJSON *list_func = cJSON_AddObjectToObject(list_tool, "function");
    cJSON_AddStringToObject(list_func, "name", "list_files");
    cJSON_AddStringToObject(list_func, "description", "List files in a directory");
    cJSON *list_params = cJSON_AddObjectToObject(list_func, "parameters");
    cJSON_AddStringToObject(list_params, "type", "object");
    cJSON *list_props = cJSON_AddObjectToObject(list_params, "properties");
    cJSON *list_path = cJSON_AddObjectToObject(list_props, "path");
    cJSON_AddStringToObject(list_path, "type", "string");
    cJSON_AddItemToArray(tools, list_tool);

    /* bash tool */
    cJSON *bash_tool = cJSON_CreateObject();
    cJSON_AddStringToObject(bash_tool, "type", "function");
    cJSON *bash_func = cJSON_AddObjectToObject(bash_tool, "function");
    cJSON_AddStringToObject(bash_func, "name", "bash");
    cJSON_AddStringToObject(bash_func, "description", "Execute a bash command");
    cJSON *bash_params = cJSON_AddObjectToObject(bash_func, "parameters");
    cJSON_AddStringToObject(bash_params, "type", "object");
    cJSON *bash_props = cJSON_AddObjectToObject(bash_params, "properties");
    cJSON *bash_cmd = cJSON_AddObjectToObject(bash_props, "command");
    cJSON_AddStringToObject(bash_cmd, "type", "string");
    cJSON *bash_req = cJSON_AddArrayToObject(bash_params, "required");
    cJSON_AddItemToArray(bash_req, cJSON_CreateString("command"));
    cJSON_AddItemToArray(tools, bash_tool);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/* Main agent loop */
static int agent_run(HttpClient *client, ToolContext *ctx, const char *user_prompt) {
    cJSON *messages = cJSON_CreateArray();

    /* Add system message */
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content",
        "You are a helpful assistant that can use tools to help users. "
        "When a user asks you to do something, use the appropriate tools.");
    cJSON_AddItemToArray(messages, sys_msg);

    /* Add user message */
    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", user_prompt);
    cJSON_AddItemToArray(messages, user_msg);

    int step = 0;
    while (step < MAX_STEPS) {
        step++;
        printf("\n[Step %d]\n", step);

        /* Build request */
        char *request_body = build_request(messages, client->model);

        /* Send to LLM */
        printf("  Calling LLM...\n");
        char *response_str = http_post(client, "/v1/chat/completions", request_body);
        free(request_body);

        if (!response_str) {
            fprintf(stderr, "Error: Failed to get response from LLM\n");
            cJSON_Delete(messages);
            return -1;
        }

        cJSON *response = cJSON_Parse(response_str);
        free(response_str);

        if (!response) {
            fprintf(stderr, "Error: Failed to parse LLM response\n");
            cJSON_Delete(messages);
            return -1;
        }

        cJSON *choices = cJSON_GetObjectItem(response, "choices");
        if (!choices || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
            fprintf(stderr, "Error: No choices in response\n");
            cJSON_Delete(response);
            cJSON_Delete(messages);
            return -1;
        }

        cJSON *choice = cJSON_GetArrayItem(choices, 0);
        cJSON *message = cJSON_GetObjectItem(choice, "message");

        /* Add assistant message to history */
        cJSON_AddItemToArray(messages, cJSON_Duplicate(message, 1));

        /* Check for content */
        cJSON *content = cJSON_GetObjectItem(message, "content");
        if (content && cJSON_IsString(content) && strlen(content->valuestring) > 0) {
            printf("  [Assistant] %s\n", content->valuestring);
        }

        /* Check for tool calls */
        cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
        if (!tool_calls || !cJSON_IsArray(tool_calls) || cJSON_GetArraySize(tool_calls) == 0) {
            /* No tool calls, we're done */
            printf("\n[Final Answer]\n%s\n", content ? content->valuestring : "(empty)");
            cJSON_Delete(response);
            cJSON_Delete(messages);
            return 0;
        }

        /* Execute tool calls */
        int num_tools = cJSON_GetArraySize(tool_calls);
        for (int i = 0; i < num_tools; i++) {
            cJSON *tool_call = cJSON_GetArrayItem(tool_calls, i);
            cJSON *id = cJSON_GetObjectItem(tool_call, "id");
            cJSON *function = cJSON_GetObjectItem(tool_call, "function");

            if (!function) continue;

            cJSON *name = cJSON_GetObjectItem(function, "name");
            cJSON *arguments_str = cJSON_GetObjectItem(function, "arguments");

            if (!name || !cJSON_IsString(name)) continue;

            cJSON *arguments = NULL;
            if (arguments_str && cJSON_IsString(arguments_str)) {
                arguments = cJSON_Parse(arguments_str->valuestring);
            }

            /* Execute tool */
            ToolResult result = execute_tool(ctx, name->valuestring, arguments);
            printf("  [Result] %s\n", result.output ? result.output : "(empty)");

            /* Add tool result to messages */
            cJSON *tool_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(tool_msg, "role", "tool");
            if (id && cJSON_IsString(id)) {
                cJSON_AddStringToObject(tool_msg, "tool_call_id", id->valuestring);
            }
            cJSON_AddStringToObject(tool_msg, "content", result.output ? result.output : "");
            cJSON_AddItemToArray(messages, tool_msg);

            free(result.output);
            if (arguments) cJSON_Delete(arguments);
        }

        cJSON_Delete(response);
    }

    fprintf(stderr, "Warning: Reached maximum steps (%d)\n", MAX_STEPS);
    cJSON_Delete(messages);
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s [options] <prompt>\n\n"
            "Lightweight agent for testing (HTTP only, minimal dependencies)\n\n"
            "Options:\n"
            "  --host <host>         LLM server host (default: 127.0.0.1)\n"
            "  --port <port>         LLM server port (default: 8080)\n"
            "  --model <id>          Model id (default: test-model)\n"
            "  --workspace <dir>     Workspace directory (default: .)\n"
            "  --api-key <key>       API key (optional)\n"
            "  --help                Show this help\n\n"
            "Example:\n"
            "  %s --host 127.0.0.1 --port 8080 \"List all files\"\n",
            argv0, argv0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    HttpClient client = {
        .host = strdup("127.0.0.1"),
        .port = 8080,
        .model = strdup("test-model"),
        .api_key = NULL
    };

    ToolContext ctx = {
        .workspace = strdup(".")
    };

    char *prompt = NULL;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            usage(argv[0]);
            return 0;
        }

        if (strncmp(arg, "--", 2) == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for %s\n", arg);
                return 2;
            }
            const char *value = argv[++i];

            if (strcmp(arg, "--host") == 0) {
                free(client.host);
                client.host = strdup(value);
            } else if (strcmp(arg, "--port") == 0) {
                client.port = atoi(value);
            } else if (strcmp(arg, "--model") == 0) {
                free(client.model);
                client.model = strdup(value);
            } else if (strcmp(arg, "--workspace") == 0) {
                free(ctx.workspace);
                ctx.workspace = strdup(value);
            } else if (strcmp(arg, "--api-key") == 0) {
                client.api_key = strdup(value);
            } else {
                fprintf(stderr, "Unknown option: %s\n", arg);
                return 2;
            }
        } else {
            if (prompt) {
                fprintf(stderr, "Multiple prompts specified\n");
                return 2;
            }
            prompt = strdup(arg);
        }
    }

    if (!prompt) {
        fprintf(stderr, "No prompt specified\n");
        usage(argv[0]);
        return 2;
    }

    printf("=== Lightweight Agent ===\n");
    printf("LLM: %s:%d\n", client.host, client.port);
    printf("Model: %s\n", client.model);
    printf("Workspace: %s\n", ctx.workspace);
    printf("Prompt: %s\n", prompt);

    int result = agent_run(&client, &ctx, prompt);

    free(client.host);
    free(client.model);
    free(client.api_key);
    free(ctx.workspace);
    free(prompt);

    return result;
}
