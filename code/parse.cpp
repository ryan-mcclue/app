// SPDX-License-Identifier: zlib-acknowledgement

INTERNAL void
gen_json_data(String8 file_name, u32 pair_count)
{
  String8List list = ZERO_STRUCT;

  MemArenaTemp temp = mem_arena_temp_begin(NULL, 0);
  MemArena *arena = temp.arena;
  
  u32 seed = linux_get_seed_u32();
  str8_list_push(arena, &list, str8_lit("{\"data\": [\n"));
  for (u32 i = 0; i < pair_count; i += 1)
  {
    u32 w0 = u32_rand_range(&seed, 20) + 1;
    u32 w1 = u32_rand_range(&seed, 20) + 1;
    f32 d0 = f32_rand_range(&seed, 150, 500);
    f32 d1 = f32_rand_range(&seed, 150, 500);

    const char *sep = (i == pair_count - 1) ? "\n" : ",\n";
    const char *fmt = "    {\"w0\": %u, \"d0\": %.2f, \"w1\": %u, \"d1\": %.2f}%s";
    String8 s = str8_fmt(arena, fmt, w0, d0, w1, d1, sep);
    str8_list_push(arena, &list, s);
  }
  str8_list_push(arena, &list, str8_lit("]}"));

  String8 data = str8_list_join(arena, list, NULL);
  str8_write_entire_file(file_name, data);

  mem_arena_temp_end(temp);
}

typedef enum
{
  JTT_EOS,
  JTT_ERROR,
  JTT_OPEN_BRACE,
  JTT_CLOSE_BRACE,
  JTT_OPEN_BRACKET,
  JTT_CLOSE_BRACKET,
  JTT_COMMA,
  JTT_COLON,
  JTT_SEMICOLON,
  JTT_STRING,
  JTT_NUMBER,
  JTT_TRUE,
  JTT_FALSE,
  JTT_NULL,

  JTT_COUNT
} JSON_TOKEN_TYPE;

typedef struct JsonToken JsonToken;
struct JsonToken
{
  JSON_TOKEN_TYPE type;
  String8 value;
};

typedef struct JsonTokenNode JsonTokenNode;
struct JsonTokenNode
{
  JsonTokenNode *next;
  JsonToken token;
};

typedef struct JsonTokenArray JsonTokenArray;
struct JsonTokenArray
{
  JsonToken *tokens;
  u32 count;
};

INTERNAL u32
parse_json_keyword(u8 *at, JsonToken *t, String8 keyword, JSON_TOKEN_TYPE type)
{
  u32 count = 0;
  u8 *keyword_start = at;
  while (count < keyword.size)
  {
    if (at[0] == keyword.content[count]) 
    {
      at += 1;
      count += 1;
    }
    else break;
  } 
  if (count == keyword.size)
  {
    t->type = type;
    t->value.content = keyword_start;
    t->value.size = keyword.size;
  }

  return count;
}

INTERNAL JsonTokenArray
lex_json(MemArena *arena, String8 str)
{
  JsonTokenNode *first = NULL, *last = NULL;

  MemArenaTemp temp_arena = mem_arena_temp_begin(NULL, 0);
  u32 token_count = 0;

  u8 *at = str.content;
  while (at && at[0]) 
  {
    while (is_whitespace(at[0])) at += 1;

    JsonToken t = ZERO_STRUCT;
    t.type = JTT_ERROR;
    t.value.content = at;
    t.value.size = 1;

    switch (at[0])
    {
      case '\0': { t.type = JTT_EOS; } break;
      case '{': { t.type = JTT_OPEN_BRACE; at += 1;} break;
      case '}': { t.type = JTT_CLOSE_BRACE; at += 1;} break;
      case '[': { t.type = JTT_OPEN_BRACKET; at += 1; } break;
      case ']': { t.type = JTT_CLOSE_BRACKET; at += 1; } break;
      case ',': { t.type = JTT_COMMA; at += 1; } break;
      case ':': { t.type = JTT_COLON; at += 1; } break;
      case ';': { t.type = JTT_SEMICOLON; at += 1; } break;
      case '"': 
      { 
        at += 1;
        u8 *string_start = at;
        while (at[0] && at[0] != '"') at += 1;
        t.value.content = string_start;
        t.value.size = at - string_start;
        if (at[0] == '"') at += 1;
        t.type = JTT_STRING; 
      } break;
      case 'f':
      {
        at += parse_json_keyword(at, &t, str8_lit("false"), JTT_FALSE);
      } break;
      case 't':
      {
        at += parse_json_keyword(at, &t, str8_lit("true"), JTT_TRUE);
      } break;
      case 'n':
      {
        at += parse_json_keyword(at, &t, str8_lit("null"), JTT_NULL);
      } break;
      case '-':
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      {
        u8 *number_start = at;
        if (at[0] == '-') at += 1;
        while (is_numeric(at[0])) at += 1;
        if (at[0] == '.') at += 1;
        while (is_numeric(at[0])) at += 1;
        t.value.content = number_start;
        t.value.size = at - number_start;
        t.type = JTT_NUMBER; 
      } break;
    }

    JsonTokenNode *n = MEM_ARENA_PUSH_STRUCT(temp_arena.arena, JsonTokenNode);
    n->token = t;
    SLL_QUEUE_PUSH(first, last, n);
    token_count += 1;
  }

  JsonTokenArray a = ZERO_STRUCT;
  a.tokens = MEM_ARENA_PUSH_ARRAY(arena, JsonToken, token_count);
  a.count = token_count;

  u32 i = 0;
  for (JsonTokenNode *n = first; n != NULL; n = n->next)
  {
    a.tokens[i++] = n->token;
  }

  mem_arena_temp_end(temp_arena);

  return a;
}

INTERNAL void
print_json_token(JsonToken t)
{
  switch (t.type)
  {
#define CASE(t) case JTT_##t: printf("JTT_"#t); break;
  CASE(ERROR)
  CASE(EOS)
  CASE(OPEN_BRACE)
  CASE(CLOSE_BRACE)
  CASE(OPEN_BRACKET)
  CASE(CLOSE_BRACKET)
  CASE(COMMA)
  CASE(COLON)
  CASE(SEMICOLON)
  CASE(STRING)
  CASE(NUMBER)
  CASE(TRUE)
  CASE(FALSE)
  CASE(NULL)
#undef CASE
  }

  printf(": %.*s\n", str8_varg(t.value));
}

typedef struct JsonParserState JsonParserState;
struct JsonParserState
{
  MemArena *arena;
  JsonTokenArray array;
  u32 at;
  b32 have_error;
};

typedef struct JsonElementNode JsonElementNode;
struct JsonElementNode
{
  JsonElementNode *next;
  String8 label;
  String8 value;
  JsonElementNode *children;
};

INTERNAL JsonToken
consume_token(JsonParserState *s)
{
  if (s->at + 1 <= s->array.count) return s->array.tokens[s->at++];
  else return ZERO_STRUCT;
}

INTERNAL b32
json_parsing_valid(JsonParserState *s)
{
  return (!s->have_error && s->at < s->array.count);
}

INTERNAL void
set_json_parsing_error(JsonParserState *s, const char *error)
{
  s->have_error = true;
  WARN(error);
}

INTERNAL JsonElementNode *
parse_json_token(JsonParserState *s, String8 label, JsonToken t);
INTERNAL JsonElementNode *
parse_json_nested(JsonParserState *s, JSON_TOKEN_TYPE ending_token_type)
{
  JsonElementNode *children = NULL;

  while (json_parsing_valid(s))
  {
    String8 label = ZERO_STRUCT;
    JsonToken value = consume_token(s);
    if (ending_token_type == JTT_CLOSE_BRACE)
    {
      if (value.type != JTT_STRING) { set_json_parsing_error(s, "Expected string"); break; }
      label = value.value;
      JsonToken colon = consume_token(s);
      if (colon.type != JTT_COLON) { set_json_parsing_error(s, "Expected colon"); break; }
      value = consume_token(s);
    }

    if (value.type == ending_token_type) break;
    JsonElementNode *e = parse_json_token(s, label, value);
    if (e != NULL) SLL_STACK_PUSH(children, e);
    else set_json_parsing_error(s, "Invalid token in JSON");

    JsonToken comma = consume_token(s);
    if (comma.type == ending_token_type) break;
    else if (comma.type != JTT_COMMA) set_json_parsing_error(s, "Invalid token in JSON");
  }

  return children;
}

INTERNAL JsonElementNode *
parse_json_token(JsonParserState *s, String8 label, JsonToken t)
{
  JsonElementNode *children = NULL;

  b32 valid = true;
  switch (t.type)
  {
    default: { valid = false; } break;
    case JTT_OPEN_BRACE: { children = parse_json_nested(s, JTT_CLOSE_BRACE); } break;
    case JTT_OPEN_BRACKET: { children = parse_json_nested(s, JTT_CLOSE_BRACKET); } break;
    case JTT_STRING:
    case JTT_NUMBER:
    case JTT_TRUE:
    case JTT_FALSE:
    case JTT_NULL:
    {} break;
  }

  JsonElementNode *result = NULL;
  if (valid)
  {
    result = MEM_ARENA_PUSH_STRUCT_ZERO(s->arena, JsonElementNode);
    result->label = label;
    result->value = t.value;
    result->children = children;
  }

  return result;
}

INTERNAL JsonElementNode *
parse_json(JsonTokenArray a, MemArena *arena)
{
  JsonParserState s = {arena, a, 0};
  String8 root_label = ZERO_STRUCT;
  JsonToken first_token = consume_token(&s);

  return parse_json_token(&s, root_label, first_token);
}

INTERNAL JsonElementNode *
lookup_label_json(JsonElementNode *o, String8 label)
{
  if (o == NULL) return NULL;

  JsonElementNode *result = NULL;
  for (JsonElementNode *n = o->children; n != NULL; n = n->next)
  {
    if (str8_match(n->label, label, 0))
    {
      result = n;
      break;
    }
  }

  return result;
}

typedef struct JsonDataArray JsonDataArray;
struct JsonDataArray
{
  Vector4 *data;
  u32 count;
};

typedef struct Vector4Node Vector4Node;
struct Vector4Node
{
  Vector4Node *next;
  Vector4 value;
};

INTERNAL f32
convert_json_object_element_to_f32(JsonElementNode *e, String8 label)
{
  JsonElementNode *n = lookup_label_json(e, label);
  if (n == NULL) return 0.f;
  return str8_to_real(n->value);
}

INTERNAL JsonDataArray
parse_json_data(MemArena *arena, JsonElementNode *data)
{
  if (data == NULL) return ZERO_STRUCT;

  Vector4Node *first = NULL;
  u32 count = 0;
  MemArenaTemp temp = mem_arena_temp_begin(NULL, 0);
  MemArena *temp_arena = temp.arena;
  for (JsonElementNode *e = data->children; e != NULL; e = e->next)
  {
    Vector4Node *n = MEM_ARENA_PUSH_STRUCT(temp_arena, Vector4Node);
    n->value.x = convert_json_object_element_to_f32(e, str8_lit("w0"));
    n->value.y = convert_json_object_element_to_f32(e, str8_lit("d0"));
    n->value.z = convert_json_object_element_to_f32(e, str8_lit("w1"));
    n->value.w = convert_json_object_element_to_f32(e, str8_lit("d1"));

    SLL_STACK_PUSH(first, n);
    count += 1;
  }

  JsonDataArray result = ZERO_STRUCT;
  result.data = MEM_ARENA_PUSH_ARRAY(arena, Vector4, count);
  result.count = count;

  u32 j = 0;
  for (Vector4Node *n = first; n != NULL; n = n->next)
  {
    result.data[j++] = n->value;
  }

  mem_arena_temp_end(temp);

  return result;
}
