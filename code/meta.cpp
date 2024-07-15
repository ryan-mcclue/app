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

enum TOKEN_TYPE {
  TOKEN_TYPE_NULL,
};

struct Token {
  TOKEN_TYPE type;
  RangeU32 range;
};

struct TokenArray {
  Token *tokens;
  u32 size;
};

INTERNAL TokenArray
lex_token_array(MemArena *arena, String8 s)
{
  TokenArray result = ZERO_STRUCT;

  TOKEN_TYPE active_token_type = TOKEN_TYPE_NULL;
  u32 at = 0;
  u32 offset = 0;
  while (offset <= s.size) 
  {
    switch (active_token_type)
    {

    }
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
