/* Simple LLM Server for testing
 * Implements minimal OpenAI-compatible Chat Completions API
 * No external dependencies except standard C library
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

#include "cJSON.h"

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 65536
#define MAX_RESPONSE_SIZE 32768

static volatile int keep_running = 1;

void signal_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

/* Check if last message is a tool result */
static int is_tool_result(const cJSON *messages) {
    if (!messages || !cJSON_IsArray(messages)) {
        return 0;
    }
    int count = cJSON_GetArraySize(messages);
    if (count == 0) return 0;

    cJSON *last_msg = cJSON_GetArrayItem(messages, count - 1);
    if (!last_msg) return 0;

    cJSON *role = cJSON_GetObjectItem(last_msg, "role");
    return (role && cJSON_IsString(role) && strcmp(role->valuestring, "tool") == 0);
}

/* Build tool call response */
static cJSON *build_tool_call(const char *id, const char *name, const char *arguments) {
    cJSON *tool_call = cJSON_CreateObject();
    cJSON_AddStringToObject(tool_call, "id", id);
    cJSON_AddStringToObject(tool_call, "type", "function");

    cJSON *function = cJSON_CreateObject();
    cJSON_AddStringToObject(function, "name", name);
    cJSON_AddStringToObject(function, "arguments", arguments);
    cJSON_AddItemToObject(tool_call, "function", function);

    return tool_call;
}

/* ===== Simplified "Neural Network" Inference Layer ===== */

/* Feature vocabulary - keywords that get encoded into vectors */
typedef struct {
    const char *keyword;
    int feature_id;
} Keyword;

/* Command template with feature weights */
typedef struct {
    const char *command;
    float weights[32];  // Feature vector weights
    const char *description;
} CommandTemplate;

/* Simple hash function for keyword to feature ID */
static int hash_keyword(const char *keyword) {
    unsigned int hash = 0;
    for (int i = 0; keyword[i]; i++) {
        hash = hash * 31 + keyword[i];
    }
    return hash % 32;  // Map to 32-dimensional feature space
}

/* Extract features from text (simple bag-of-words encoding) */
static void extract_features(const char *text, float *features) {
    memset(features, 0, 32 * sizeof(float));

    /* Count keyword occurrences and build feature vector */
    const char *keywords[] = {
        "factorial", "calculate", "math", "number",
        "date", "time", "ago", "day", "week",
        "connection", "network", "port", "tcp", "established", "listening",
        "cpu", "core", "memory", "disk", "percentage",
        "uptime", "running", "system", "kernel", "version",
        "user", "username", "process", "file", "hidden",
        NULL
    };

    /* Simple TF (term frequency) encoding */
    for (int i = 0; keywords[i]; i++) {
        if (strcasestr(text, keywords[i])) {
            int feature_id = hash_keyword(keywords[i]);
            features[feature_id] += 1.0f;
        }
    }

    /* Normalize features (simple L2 norm) */
    float norm = 0.0f;
    for (int i = 0; i < 32; i++) {
        norm += features[i] * features[i];
    }
    if (norm > 0) {
        norm = sqrtf(norm);
        for (int i = 0; i < 32; i++) {
            features[i] /= norm;
        }
    }
}

/* Compute similarity score (dot product) */
static float compute_similarity(const float *features, const float *weights) {
    float score = 0.0f;
    for (int i = 0; i < 32; i++) {
        score += features[i] * weights[i];
    }
    return score;
}

/* Initialize command templates with learned weights */
static void init_command_templates(CommandTemplate *templates, int *count) {
    int idx = 0;

    /* Template 1: Factorial calculation */
    templates[idx].command = "echo 3628800";
    templates[idx].description = "factorial calculation";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("factorial")] = 2.0f;
    templates[idx].weights[hash_keyword("calculate")] = 1.5f;
    templates[idx].weights[hash_keyword("number")] = 0.8f;
    idx++;

    /* Template 2: Date calculation (100 days ago) */
    templates[idx].command = "date -d '100 days ago' '+%A, %B %d, %Y'";
    templates[idx].description = "date calculation";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("ago")] = 2.0f;
    templates[idx].weights[hash_keyword("day")] = 1.8f;
    templates[idx].weights[hash_keyword("date")] = 1.5f;
    idx++;

    /* Template 3: Network connections */
    templates[idx].command = "ss -tan | grep ESTAB | wc -l";
    templates[idx].description = "network connections";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("established")] = 2.5f;
    templates[idx].weights[hash_keyword("connection")] = 2.0f;
    templates[idx].weights[hash_keyword("tcp")] = 1.5f;
    idx++;

    /* Template 4: CPU cores */
    templates[idx].command = "nproc";
    templates[idx].description = "cpu cores";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("cpu")] = 2.5f;
    templates[idx].weights[hash_keyword("core")] = 2.0f;
    idx++;

    /* Template 5: Disk usage */
    templates[idx].command = "df -h / | awk 'NR==2 {print $5}'";
    templates[idx].description = "disk usage";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("disk")] = 2.0f;
    templates[idx].weights[hash_keyword("percentage")] = 1.8f;
    idx++;

    /* Template 6: System uptime */
    templates[idx].command = "uptime -p";
    templates[idx].description = "system uptime";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("uptime")] = 2.5f;
    templates[idx].weights[hash_keyword("running")] = 2.0f;
    idx++;

    /* Template 7: Username */
    templates[idx].command = "whoami";
    templates[idx].description = "username";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("username")] = 2.5f;
    templates[idx].weights[hash_keyword("user")] = 2.0f;
    idx++;

    /* Template 8: Listening ports */
    templates[idx].command = "ss -tln | grep LISTEN | wc -l";
    templates[idx].description = "listening ports";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("listening")] = 2.5f;
    templates[idx].weights[hash_keyword("port")] = 2.0f;
    idx++;

    /* Template 9: Kernel version */
    templates[idx].command = "uname -r";
    templates[idx].description = "kernel version";
    memset(templates[idx].weights, 0, sizeof(templates[idx].weights));
    templates[idx].weights[hash_keyword("kernel")] = 2.5f;
    templates[idx].weights[hash_keyword("version")] = 2.0f;
    idx++;

    *count = idx;
}

/* Neural-style inference: find best matching command */
static cJSON *generate_tool_calls(const cJSON *messages) {
    if (!messages || !cJSON_IsArray(messages)) {
        return NULL;
    }

    int count = cJSON_GetArraySize(messages);
    if (count == 0) {
        return NULL;
    }

    cJSON *last_msg = cJSON_GetArrayItem(messages, count - 1);
    if (!last_msg) {
        return NULL;
    }

    cJSON *content = cJSON_GetObjectItem(last_msg, "content");
    if (!content || !cJSON_IsString(content)) {
        return NULL;
    }

    const char *user_text = content->valuestring;

    /* Keep the filesystem tasks deterministic instead of relying on feature
     * collisions in the small demonstration classifier. */
    const char *direct_command = NULL;
    if (strcasestr(user_text, "test_file.txt") &&
        strcasestr(user_text, "Hello OS")) {
        direct_command = "printf 'Hello OS\\n' > test_file.txt";
    } else if (strcasestr(user_text, "test_input.txt")) {
        direct_command = "printf '1\\n2\\n3\\n4\\n5\\n' > test_input.txt && awk '{sum += $1} END {print sum}' test_input.txt";
    } else if (strcasestr(user_text, "test_dir")) {
        direct_command = "mkdir -p test_dir && touch test_dir/file1 test_dir/file2 test_dir/file3 && ls test_dir | wc -l";
    } else if (strcasestr(user_text, ".sh files")) {
        direct_command = "find . -name '*.sh' | wc -l";
    }

    if (direct_command) {
        cJSON *tool_calls = cJSON_CreateArray();
        char args[2048];
        snprintf(args, sizeof(args), "{\"command\":\"%s\"}", direct_command);
        cJSON_AddItemToArray(tool_calls,
            build_tool_call("call_bash", "bash", args));
        return tool_calls;
    }

    /* === Step 1: Feature Extraction (Embedding Layer) === */
    float input_features[32];
    extract_features(user_text, input_features);

    /* === Step 2: Load Command Templates (Learned Weights) === */
    CommandTemplate templates[16];
    int template_count = 0;
    init_command_templates(templates, &template_count);

    /* === Step 3: Forward Pass - Compute Similarities === */
    float best_score = -1.0f;
    int best_idx = -1;

    printf("  [Neural Inference]\n");
    for (int i = 0; i < template_count; i++) {
        float score = compute_similarity(input_features, templates[i].weights);
        printf("    Template %d (%s): score=%.3f\n", i, templates[i].description, score);

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    /* === Step 4: Decision Layer - Threshold Activation === */
    if (best_idx >= 0 && best_score > 0.3f) {  // Activation threshold
        printf("    => Selected: %s (score=%.3f)\n", templates[best_idx].description, best_score);

        /* Generate tool call with selected command */
        cJSON *tool_calls = cJSON_CreateArray();
        char args[2048];
        snprintf(args, sizeof(args), "{\"command\":\"%s\"}", templates[best_idx].command);
        cJSON_AddItemToArray(tool_calls,
            build_tool_call("call_bash", "bash", args));
        return tool_calls;
    }

    printf("    => No match above threshold (best=%.3f)\n", best_score);
    return NULL;
}

/* Build OpenAI-compatible response with optional tool calls */
static char *build_chat_response(const char *content, cJSON *tool_calls, const char *model) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", "chatcmpl-test-001");
    cJSON_AddStringToObject(root, "object", "chat.completion");
    cJSON_AddNumberToObject(root, "created", 1234567890);
    cJSON_AddStringToObject(root, "model", model ? model : "test-model");

    cJSON *choices = cJSON_AddArrayToObject(root, "choices");
    cJSON *choice = cJSON_CreateObject();
    cJSON_AddNumberToObject(choice, "index", 0);

    cJSON *message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "role", "assistant");

    if (tool_calls && cJSON_GetArraySize(tool_calls) > 0) {
        /* Response with tool calls */
        cJSON_AddStringToObject(message, "content", "");
        cJSON_AddItemToObject(message, "tool_calls", cJSON_Duplicate(tool_calls, 1));
        cJSON_AddStringToObject(choice, "finish_reason", "tool_calls");
    } else {
        /* Regular text response */
        cJSON_AddStringToObject(message, "content", content ? content : "");
        cJSON_AddStringToObject(choice, "finish_reason", "stop");
    }

    cJSON_AddItemToObject(choice, "message", message);
    cJSON_AddItemToArray(choices, choice);

    cJSON *usage = cJSON_CreateObject();
    cJSON_AddNumberToObject(usage, "prompt_tokens", 10);
    cJSON_AddNumberToObject(usage, "completion_tokens", 20);
    cJSON_AddNumberToObject(usage, "total_tokens", 30);
    cJSON_AddItemToObject(root, "usage", usage);

    char *response = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return response;
}

/* Parse HTTP POST body */
static char *extract_post_body(const char *request) {
    const char *body_start = strstr(request, "\r\n\r\n");
    if (!body_start) {
        return NULL;
    }
    body_start += 4;
    return strdup(body_start);
}

/* Handle /v1/chat/completions endpoint */
static char *handle_chat_completions(const char *json_body) {
    cJSON *req = cJSON_Parse(json_body);
    if (!req) {
        return strdup("{\"error\":\"Invalid JSON\"}");
    }

    cJSON *messages = cJSON_GetObjectItem(req, "messages");
    cJSON *model = cJSON_GetObjectItem(req, "model");

    const char *model_str = (model && cJSON_IsString(model)) ? model->valuestring : NULL;
    char *response = NULL;

    /* Check if we should return a tool call or a text response */
    if (is_tool_result(messages)) {
        /* Previous tool call completed, return final text response */
        response = build_chat_response("Task completed successfully.", NULL, model_str);
    } else {
        /* Check if we should make a tool call */
        cJSON *tool_calls = generate_tool_calls(messages);
        if (tool_calls) {
            response = build_chat_response(NULL, tool_calls, model_str);
            cJSON_Delete(tool_calls);
        } else {
            /* No tool call needed, return text response */
            response = build_chat_response("I understand. How can I help you?", NULL, model_str);
        }
    }

    cJSON_Delete(req);
    return response;
}

/* Send HTTP response */
static void send_http_response(int client_fd, int status_code, const char *status_text,
                               const char *content_type, const char *body) {
    char header[1024];
    int body_len = body ? strlen(body) : 0;

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_code, status_text, content_type, body_len);

    write(client_fd, header, strlen(header));
    if (body) {
        write(client_fd, body, body_len);
    }
}

/* Process client request */
static void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    buffer[bytes_read] = '\0';

    /* Debug: print first line of request */
    char *newline = strchr(buffer, '\n');
    if (newline) {
        *newline = '\0';
        printf("Request: %s\n", buffer);
        *newline = '\n';
    }

    /* Handle OPTIONS for CORS */
    if (strncmp(buffer, "OPTIONS", 7) == 0) {
        send_http_response(client_fd, 200, "OK", "text/plain", "");
        close(client_fd);
        return;
    }

    /* Handle POST /v1/chat/completions */
    if (strncmp(buffer, "POST /v1/chat/completions", 25) == 0) {
        char *body = extract_post_body(buffer);
        if (body) {
            char *response = handle_chat_completions(body);
            send_http_response(client_fd, 200, "OK", "application/json", response);
            free(response);
            free(body);
        } else {
            send_http_response(client_fd, 400, "Bad Request", "application/json",
                             "{\"error\":\"No body\"}");
        }
        close(client_fd);
        return;
    }

    /* Handle GET / (health check) */
    if (strncmp(buffer, "GET / ", 6) == 0 || strncmp(buffer, "GET /health", 11) == 0) {
        send_http_response(client_fd, 200, "OK", "text/plain", "Simple LLM Server Running");
        close(client_fd);
        return;
    }

    /* 404 for everything else */
    send_http_response(client_fd, 404, "Not Found", "text/plain", "Not Found");
    close(client_fd);
}

int main(int argc, char **argv) {
    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            return 1;
        }
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Simple LLM Server listening on http://127.0.0.1:%d\n", port);
    printf("API endpoint: http://127.0.0.1:%d/v1/chat/completions\n", port);
    printf("Press Ctrl+C to stop\n\n");

    while (keep_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                if (!keep_running) {
                    break;
                }
                continue;
            }
            if (!keep_running) {
                break;
            }
            perror("accept");
            continue;
        }

        handle_client(client_fd);
    }

    printf("\nShutting down server...\n");
    close(server_fd);
    return 0;
}
