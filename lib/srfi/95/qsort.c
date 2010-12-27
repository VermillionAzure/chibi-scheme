/*  qsort.c -- quicksort implementation                       */
/*  Copyright (c) 2009-2010 Alex Shinn.  All rights reserved. */
/*  BSD-style license: http://synthcode.com/license.txt       */

#include "chibi/eval.h"

#if SEXP_USE_HUFF_SYMS
#include "../../../opt/sexp-hufftabs.c"
#endif

#define swap(tmp_var, a, b) (tmp_var=a, a=b, b=tmp_var)

static sexp sexp_vector_copy_to_list (sexp ctx, sexp vec, sexp seq) {
  sexp_sint_t i;
  sexp ls, *data=sexp_vector_data(vec);
  for (i=0, ls=seq; sexp_pairp(ls); i++, ls=sexp_cdr(ls))
    sexp_car(ls) = data[i];
  return seq;
}

static sexp sexp_vector_nreverse (sexp ctx, sexp vec) {
  int i, j;
  sexp tmp, *data=sexp_vector_data(vec);
  for (i=0, j=sexp_vector_length(vec)-1; i<j; i++, j--)
    swap(tmp, data[i], data[j]);
  return vec;
}

static int sexp_basic_comparator (sexp op) {
  if (sexp_not(op))
    return 1;
  if (! sexp_opcodep(op))
    return 0;
  if (sexp_opcode_class(op) == SEXP_OPC_ARITHMETIC_CMP)
    return 1;
  return 0;
}

#if SEXP_USE_HUFF_SYMS
static int sexp_isymbol_compare (sexp ctx, sexp a, sexp b) {
  int res, res2, tmp;
  sexp_uint_t c = ((sexp_uint_t)a)>>3, d = ((sexp_uint_t)b)>>3;
  while (c && d) {
#include "../../../opt/sexp-unhuff.c"
#define c d
#define res res2
#include "../../../opt/sexp-unhuff.c"
#undef c
#undef res
    if ((tmp=res-res2) != 0)
      return tmp;
  }
  return c ? 1 : d ? -1 : 0;
}
#endif

static int sexp_object_compare (sexp ctx, sexp a, sexp b) {
  int res;
  if (a == b)
    return 0;
  if (sexp_pointerp(a)) {
    if (sexp_pointerp(b)) {
      if (sexp_pointer_tag(a) != sexp_pointer_tag(b)) {
        res = sexp_pointer_tag(a) - sexp_pointer_tag(b);
      } else {
        switch (sexp_pointer_tag(a)) {
        case SEXP_FLONUM:
          res = sexp_flonum_value(a) - sexp_flonum_value(b);
          break;
        case SEXP_BIGNUM:
          res = sexp_bignum_compare(a, b);
          break;
        case SEXP_STRING:
          res = strcmp(sexp_string_data(a), sexp_string_data(b));
          break;
        case SEXP_SYMBOL:
          res = strcmp(sexp_symbol_data(a), sexp_symbol_data(b));
          break;
        default:
          res = 0;
          break;
        }
      }
#if SEXP_USE_HUFF_SYMS
    } else if (sexp_lsymbolp(a) && sexp_isymbolp(b)) {
      res = strcmp(sexp_symbol_data(a),
		   sexp_string_data(sexp_write_to_string(ctx, b)));
#endif
    } else {
      res = 1;
    }
  } else if (sexp_pointerp(b)) {
#if SEXP_USE_HUFF_SYMS
    if (sexp_isymbolp(a) && sexp_lsymbolp(b))
      res = strcmp(sexp_string_data(sexp_write_to_string(ctx, a)),
		   sexp_symbol_data(b));
    else
#endif
      res = -1;
  } else {
#if SEXP_USE_HUFF_SYMS
    if (sexp_isymbolp(a) && sexp_isymbolp(b))
      return sexp_isymbol_compare(ctx, a, b);
    else
#endif
      res = (sexp_sint_t)a - (sexp_sint_t)b;
  }
  return res;
}

static sexp sexp_object_compare_op (sexp ctx sexp_api_params(self, n), sexp a, sexp b) {
  return sexp_make_fixnum(sexp_object_compare(ctx, a, b));
}

/* fast path when using general object-cmp comparator with no key */
static void sexp_qsort (sexp ctx, sexp *vec, sexp_sint_t lo, sexp_sint_t hi) {
  sexp_sint_t mid, i, j;
  sexp tmp, tmp2;
 loop:
  if (lo < hi) {
    mid = lo + (hi-lo)/2;
    swap(tmp, vec[mid], vec[hi]);
    for (i=j=lo; i < hi; i++)
      if (sexp_object_compare(ctx, vec[i], tmp) < 0)
        swap(tmp2, vec[i], vec[j]), j++;
    swap(tmp, vec[j], vec[hi]);
    if ((hi-lo) > 2) {
      sexp_qsort(ctx, vec, lo, j-1);
      lo = j;
      goto loop; /* tail recurse on right side */
    }
  }
}

static sexp sexp_qsort_less (sexp ctx, sexp *vec,
                             sexp_sint_t lo, sexp_sint_t hi,
                             sexp less, sexp key) {
  sexp_sint_t mid, i, j;
  sexp tmp, res, args1;
  sexp_gc_var3(a, b, args2);
  sexp_gc_preserve3(ctx, a, b, args2);
  args2 = sexp_list2(ctx, SEXP_VOID, SEXP_VOID);
  args1 = sexp_cdr(args2);
 loop:
  if (lo >= hi) {
    res = SEXP_VOID;
  } else {
    mid = lo + (hi-lo)/2;
    swap(tmp, vec[mid], vec[hi]);
    if (sexp_truep(key)) {
      sexp_car(args1) = tmp;
      b = sexp_apply(ctx, key, args1);
    } else {
      b = tmp;
    }
    for (i=j=lo; i < hi; i++) {
      if (sexp_truep(key)) {
        sexp_car(args1) = vec[i];
        a = sexp_apply(ctx, key, args1);
      } else {
        a = vec[i];
      }
      sexp_car(args2) = a;
      sexp_car(args1) = b;
      res = sexp_apply(ctx, less, args2);
      if (sexp_exceptionp(res))
        goto done;
      else if (sexp_truep(res))
        swap(res, vec[i], vec[j]), j++;
    }
    swap(tmp, vec[j], vec[hi]);
    if ((hi-lo) > 2) {
      res = sexp_qsort_less(ctx, vec, lo, j-1, less, key);
      if (sexp_exceptionp(res))
        goto done;
      lo = j;
      goto loop; /* tail recurse on right side */
    }
  }
 done:
  sexp_gc_release3(ctx);
  return res;
}

static sexp sexp_sort_x (sexp ctx sexp_api_params(self, n), sexp seq,
                         sexp less, sexp key) {
  sexp_sint_t len;
  sexp res, *data;
  sexp_gc_var1(vec);

  if (sexp_nullp(seq)) return seq;

  sexp_gc_preserve1(ctx, vec);

  vec = (sexp_truep(sexp_listp(ctx, seq)) ? sexp_list_to_vector(ctx, seq) : seq);

  if (! sexp_vectorp(vec)) {
    res = sexp_type_exception(ctx, self, SEXP_VECTOR, vec);
  } else {
    data = sexp_vector_data(vec);
    len = sexp_vector_length(vec);
    if (sexp_not(key) && sexp_basic_comparator(less)) {
      sexp_qsort(ctx, data, 0, len-1);
      if (sexp_opcodep(less) && sexp_opcode_inverse(less))
        sexp_vector_nreverse(ctx, vec);
      res = vec;
    } else if (! (sexp_procedurep(less) || sexp_opcodep(less))) {
      res = sexp_type_exception(ctx, self, SEXP_PROCEDURE, less);
    } else if (! (sexp_procedurep(key) || sexp_opcodep(key) || sexp_not(key))) {
      res = sexp_type_exception(ctx, self, SEXP_PROCEDURE, key);
    } else {
      res = sexp_qsort_less(ctx, data, 0, len-1, less, key);
    }
  }

  if (sexp_pairp(seq) && ! sexp_exceptionp(res))
    res = sexp_vector_copy_to_list(ctx, vec, seq);

  sexp_gc_release1(ctx);
  return res;
}

sexp sexp_init_library (sexp ctx sexp_api_params(self, n), sexp env) {
  sexp_define_foreign(ctx, env, "object-cmp", 2, sexp_object_compare_op);
  sexp_define_foreign_opt(ctx, env, "sort!", 3, sexp_sort_x, SEXP_FALSE);
  return SEXP_VOID;
}