// SPDX-License-Identifier: zlib-acknowledgement
#include "base/base-inc.h"

//typedef struct MD_ParseResult MD_ParseResult;
//struct MD_ParseResult
//{
//    MD_Node *node;
//    MD_u64 string_advance;
//    MD_MessageList errors;
//};
//MD_ParseResult parse = MD_ParseWholeString(arena, filename, file_content);
//
//MD_FUNCTION MD_ParseResult
//MD_ParseWholeString(MD_Arena *arena, MD_String8 filename, MD_String8 contents)
//{
//    MD_Node *root = MD_MakeNode(arena, MD_NodeKind_File, filename, contents, 0);
//    MD_ParseResult result = MD_ParseNodeSet(arena, contents, 0, root, MD_ParseSetRule_Global);
//    result.node = root;
//    for(MD_Message *error = result.errors.first; error != 0; error = error->next)
//    {
//        if(MD_NodeIsNil(error->node->parent))
//        {
//            error->node->parent = root;
//        }
//    }
//    return result;
//}

INTROSPECT(category: "hi") struct Name
{
  int x;
  char *y;
};

enum TOKEN_TYPE {
  TOKEN_TYPE_NULL,
  TOKEN_TYPE_IDENTIFIER,
  TOKEN_TYPE_OPEN_PAREN,
  TOKEN_TYPE_CLOSE_PAREN,
  TOKEN_TYPE_OPEN_BRACE,
  TOKEN_TYPE_CLOSE_BRACE,
  TOKEN_TYPE_OPEN_BRACKET,
  TOKEN_TYPE_CLOSE_BRACKET,
  TOKEN_TYPE_COLON,
  TOKEN_TYPE_STRING,
  TOKEN_TYPE_ASTERISK,
  TOKEN_TYPE_SEMICOLON,
  TOKEN_EOS
};

struct Token {
  TOKEN_TYPE type;
  RangeU32 range;
};

struct TokenArray {
  Token *tokens;
  u32 size;
};

typedef struct TokenNode TokenNode;
struct TokenNode {
  TokenNode *next;
  Token token;
};

INTERNAL TokenArray
lex_token_array(MemArena *arena, String8 str)
{
  Token *first = NULL, *last = NULL;

  TOKEN_TYPE token_type = TOKEN_TYPE_NULL;
  u32 token_start = 0;

  char *at = str.content;
  while (at[0]) 
  {
    while (is_whitespace(at[0]))
    {
      at += 1;
    }
    if (at[0] == '\\' && at[1] == '\\')
    {
      at += 2;
      while (at[0] && at[0] != '\n' && at[0] != '\r')
      {
        at += 1;
      }
    } 
    else if (at[0] == '/' && at[1] == '*')
    {
      at += 2;
      while (at[0] && at[0] != '*' && at[1] && at[1] != '/')
      {
        at += 1;
      }
    }

    u32 token_start = at - str.content;
    switch (at)
    {
      case '\0': { token_type = TOKEN_TYPE_EOS; } break;
      case '(': { token_type = TOKEN_TYPE_OPEN_PAREN; } break;
      case ')': { token_type = TOKEN_TYPE_CLOSE_PAREN; } break;
      case '{': { token_type = TOKEN_TYPE_OPEN_BRACE; } break;
      case '}': { token_type = TOKEN_TYPE_CLOSE_BRACE; } break;
      case '[': { token_type = TOKEN_TYPE_OPEN_BRACKET; } break;
      case ']': { token_type = TOKEN_TYPE_CLOSE_BRACKET; } break;
      case ';': { token_type = TOKEN_TYPE_SEMICOLON; } break;
      case ':': { token_type = TOKEN_TYPE_COLON; } break;
      case '*': { token_type = TOKEN_TYPE_ASTERISK; } break;
      case '"': 
      { 
        token_type = TOKEN_TYPE_STRING; 
        at += 1;
        token_start = at - str.content;
        while (at[0] && at[0] != '"')
        {
          if (at[0] == '\\' && at[1])
          {
            at += 1;
          }
          at += 1;
        }
      } break;
      default:
      {
        if (is_alpha(ch))
        {
          lex_identifier();
        }
        else if (is_numeric(ch))
        {
          lex_number();
        }
        else
        {
          token_type = TOKEN_TYPE_NULL;
        }
      } break;
    }

    u32 token_end = token_start.content - at;
    TokenNode *token = MEM_ARENA_PUSH_STRUCT(temp_arena, TokenNode);
    token->type = token_type;
    token->range = range_u32(token_start, token_end);
    SLL_QUEUE_PUSH(first, last, token);
    token_count += 1;

    token_type = TOKEN_TYPE_NULL;
  }

  return result;
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

  String8 f = str8_read_entire_file(arena, str8_lit("code/app.h"));

  TokenArray token_array = lex_token_array(arena, f);

  return 0;
}
