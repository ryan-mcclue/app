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
      case '{': { t.type = JTT_OPEN_BRACE; } break;
      case '}': { t.type = JTT_CLOSE_BRACE; } break;
      case '[': { t.type = JTT_OPEN_BRACKET; } break;
      case ']': { t.type = JTT_CLOSE_BRACKET; } break;
      case ',': { t.type = JTT_COMMA; } break;
      case ':': { t.type = JTT_COLON; } break;
      case ';': { t.type = JTT_SEMICOLON; } break;
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
    n->token= t;
    SLL_QUEUE_PUSH(first, last, n);
    token_count += 1;
    at += (t.value.size == 1 ? 1 : 0);
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

INTERNAL JsonElementNode
parse_json(JsonToken)
{

}

/* INTERNAL bool
token_equals(String8 f, JsonToken t, String8 m)
{
  String8 src = str8(f.content + t.range.min, range_u32_dim(t.range));
  return str8_match(src, m, 0);
}

INTERNAL JsonToken
consume_token(JsonTokeniser *t)
{
  if (t->at + 1 < t->array.count)
  {
    t->at += 1;
    return t->array.tokens[t->at - 1];
  }
  else
  {
    return ZERO_STRUCT;
  }
}

INTERNAL bool
require_token(JsonTokeniser *t, TOKEN_TYPE type)
{
  JsonToken token = consume_token(t);
  return (token.type == type);
}

INTERNAL void
parse_introspectable_params(JsonTokeniser *t)
{
  while (true)
  {
    JsonToken token = consume_token(t);
    if (token.type == TOKEN_TYPE_CLOSE_PAREN ||
        token.type == TOKEN_TYPE_EOS) break;
  }
}

INTERNAL void
parse_struct_member(JsonTokeniser *t, JsonToken struct_type, JsonToken member_type)
{
  bool is_pointer = false;
  while (true)
  {
    JsonToken token = consume_token(t);
    if (token.type == TOKEN_TYPE_ASTERISK)
    {
      is_pointer = true;
    }
    if (token.type == TOKEN_TYPE_IDENTIFIER)
    {
      printf("    {%s, meta_type_%.*s, \"%.*s\", (uintptr_t)&(((%.*s *)0)->%.*s)}, \n", 
              is_pointer ? "MEMBER_TYPE_IS_POINTER" : "0",
              TOKEN_VARG(t->f, member_type),
              TOKEN_VARG(t->f, token),
              TOKEN_VARG(t->f, struct_type),
              TOKEN_VARG(t->f, token));
    }
    else if (token.type == TOKEN_TYPE_SEMICOLON ||
        token.type == TOKEN_TYPE_EOS) break;
  }
}


INTERNAL void
parse_struct(JsonTokeniser *t)
{
  JsonToken name = consume_token(t);
  if (require_token(t, TOKEN_TYPE_OPEN_BRACE))
  {
    printf("GLOBAL MemberDefinition g_members_of_%.*s[] = \n", TOKEN_VARG(t->f, name));
    printf("{\n");
    while (true)
    {
      JsonToken member_token = consume_token(t);
      if (member_token.type == TOKEN_TYPE_CLOSE_BRACE ||
          member_token.type == TOKEN_TYPE_EOS)
      {
        break;
      }
      else
      {
        parse_struct_member(t, name, member_token);
      }
    }
    printf("};\n");

    MetaStruct *s = MEM_ARENA_PUSH_STRUCT(t->arena, MetaStruct);
    s->name = str8_fmt(t->arena, "%.*s", TOKEN_VARG(t->f, name));
    SLL_STACK_PUSH(first_meta_struct, s);
  }
  else
  {
    WARN("struct requires {");
  }
}

INTERNAL void
parse_introspectable(JsonTokeniser *t)
{
  if (require_token(t, TOKEN_TYPE_OPEN_PAREN))
  {
    parse_introspectable_params(t);
    JsonToken token = consume_token(t);
    if (token_equals(t->f, token, str8_lit("struct")))
    {
      parse_struct(t);
    }
    else
    {
      WARN("instropect just for structs");
    }
  }
  else
  {
    WARN("Require ( after instropect");
  }
}

int
main(int argc, char *argv[])
{
  global_debugger_present = linux_was_launched_by_gdb();
  MemArena *arena = mem_arena_allocate(GB(8), MB(64));

  ThreadContext tctx = thread_context_allocate(GB(8), MB(64));
  tctx.is_main_thread = true;
  thread_context_set(&tctx);
  thread_context_set_name("Main Thread");

  String8 f = str8_read_entire_file(arena, str8_lit("code/meta-test.h"));
  JsonTokeniser tokeniser = lex(arena, f);
  tokeniser.arena = arena;

  while (true)
  {
    JsonToken t = consume_token(&tokeniser); 

    //print_token(f, t);

    if (t.type == TOKEN_TYPE_EOS) break;
    else if (token_equals(f, t, str8_lit("introspect")))
    {
      parse_introspectable(&tokeniser);
    }
  }

  printf("#define META_STRUCT_DUMP(member_ptr) \\ \n");
  for (MetaStruct *s = first_meta_struct; s != NULL; s = s->next)
  {
    printf("case meta_type_%.*s: dump_struct(member_ptr, members_of_%.*s, ARRAY_COUNT(members_of_%.*s), indent_level + 1); break; %s\n", 
            str8_varg(s->name), str8_varg(s->name), str8_varg(s->name), 
            s->next ? "\\" : "");
  }

  return 0;
} */
