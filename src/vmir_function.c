


/**
 *
 */
static ir_bb_t *
bb_find(ir_function_t *f,  int id)
{
  ir_bb_t *ib;
  TAILQ_FOREACH(ib, &f->if_bbs, ib_link) {
    if(ib->ib_id == id)
      return ib;
  }
  return NULL;
}



/**
 *
 */
static void
ibe_destroy(ir_bb_edge_t *ibe)
{
  LIST_REMOVE(ibe, ibe_from_link);
  LIST_REMOVE(ibe, ibe_to_link);
  LIST_REMOVE(ibe, ibe_function_link);
  free(ibe);
}


/**
 *
 */
static void
ibe_destroy_list(struct ir_bb_edge_list *list)
{
  ir_bb_edge_t *ibe;
  while((ibe = LIST_FIRST(list)) != NULL)
    ibe_destroy(ibe);
}


/**
 *
 */
static void
bb_destroy(ir_bb_t *ib)
{
  ir_instr_t *ii, *next;
  ibe_destroy_list(&ib->ib_incoming_edges);
  ibe_destroy_list(&ib->ib_outgoing_edges);

  for(ii = TAILQ_FIRST(&ib->ib_instrs); ii != NULL; ii = next) {
    next = TAILQ_NEXT(ii, ii_link);
    free(ii);
  }
  free(ib);
}




/**
 *
 */
static void
function_prepare_parse(ir_unit_t *iu, ir_function_t *f)
{
  f->if_regframe_size = 8; // Make space for temporary register for VM use
  f->if_callarg_size = 0;

  ir_type_t *it = type_get(iu, f->if_type);

  for(int i = 0; i < it->it_function.num_parameters; i++)
    value_alloc_function_arg(iu, it->it_function.parameters[i]);
}


/**
 *
 */
static void
function_print(ir_unit_t *iu, ir_function_t *f, const char *what)
{
  printf("\nDump of %s function %s (%s)\n", what,
         f->if_name, type_str_index(iu, f->if_type));
  ir_bb_t *ib;
  TAILQ_FOREACH(ib, &f->if_bbs, ib_link) {
    ir_instr_t *ii;
    printf(".%d:", ib->ib_id);

    ir_bb_edge_t *ibe;
    LIST_FOREACH(ibe, &ib->ib_incoming_edges, ibe_to_link) {
      printf(" pred:%d", ibe->ibe_from->ib_id);
    }
    printf("\n");
    TAILQ_FOREACH(ii, &ib->ib_instrs, ii_link) {
      printf("\t");
      instr_print(iu, ii, 0);
      printf("\n");
    }
  }
}




/**
 *
 */
static ir_function_t *
function_find(ir_unit_t *iu, const char *name)
{
  for(int i = 0; i < VECTOR_LEN(&iu->iu_functions); i++) {
    ir_function_t *f = VECTOR_ITEM(&iu->iu_functions, i);
    if(!strcmp(f->if_name, name))
      return f;
  }
  return NULL;
}



/**
 *
 */
static void
function_remove_bb(ir_function_t *f)
{
  ir_bb_t *ib;
  while((ib = TAILQ_FIRST(&f->if_bbs)) != NULL) {
    TAILQ_REMOVE(&f->if_bbs, ib, ib_link);
    bb_destroy(ib);
  }
}


/**
 *
 */
static void
function_destroy(ir_function_t *f)
{
  function_remove_bb(f);
  free(f->if_name);
  free(f->if_vm_text);
  free(f);
}

