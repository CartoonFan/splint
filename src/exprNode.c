/*
** LCLint - annotation-assisted static program checker
** Copyright (C) 1994-2000 University of Virginia,
**         Massachusetts Institute of Technology
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version.
** 
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
** 
** The GNU General Public License is available from http://www.gnu.org/ or
** the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
** MA 02111-1307, USA.
**
** For information on lclint: lclint-request@cs.virginia.edu
** To report a bug: lclint-bug@cs.virginia.edu
** For more information: http://lclint.cs.virginia.edu
*/
/*
** exprNode.c
*/

# include <ctype.h> /* for isdigit */
# include "lclintMacros.nf"
# include "basic.h"
# include "cgrammar.h"
# include "cgrammar_tokens.h"

# include "exprChecks.h"
# include "aliasChecks.h"
# include "exprNodeSList.h"
# include "exprData.i"

static bool exprNode_isEmptyStatement (exprNode p_e);
static /*@exposed@*/ exprNode exprNode_firstStatement (/*@returned@*/ exprNode p_e);
static bool exprNode_isFalseConstant (exprNode p_e) /*@*/ ;
static bool exprNode_isBlock (exprNode p_e);
static void checkGlobUse (uentry p_glob, bool p_isCall, /*@notnull@*/ exprNode p_e);
static void exprNode_addUse (exprNode p_e, sRef p_s);
static bool exprNode_matchArgType (ctype p_ct, exprNode p_e);
static exprNode exprNode_fakeCopy (exprNode p_e) /*@*/ ;
static exprNode exprNode_statementError (/*@only@*/ exprNode p_e);
static bool exprNode_matchTypes (exprNode p_e1, exprNode p_e2);
static void checkUniqueParams (exprNode p_fcn,
			       /*@notnull@*/ exprNode p_current, exprNodeList p_args, 
			       int p_paramno, uentry p_ucurrent);
static void updateAliases (/*@notnull@*/ exprNode p_e1, /*@notnull@*/ exprNode p_e2);
static void abstractOpError (ctype p_tr1, ctype p_tr2, lltok p_op, 
			     /*@notnull@*/ exprNode p_e1, /*@notnull@*/ exprNode p_e2, 
			     fileloc p_loc1, fileloc p_loc2);
static ctype checkNumerics (ctype p_tr1, ctype p_tr2, ctype p_te1, ctype p_te2,
			    /*@notnull@*/ exprNode p_e1, /*@notnull@*/ exprNode p_e2, lltok p_op);
static void doAssign (/*@notnull@*/ exprNode p_e1, /*@notnull@*/ exprNode p_e2, bool p_isInit);
static void checkSafeUse (exprNode p_e, sRef p_s);
static void reflectNullTest (/*@notnull@*/ exprNode p_e, bool p_isnull);
static void checkMacroParen (exprNode p_e);
static exprNodeSList exprNode_flatten (/*@dependent@*/ exprNode p_e);
static void exprNode_checkSetAny (exprNode p_e, /*@dependent@*/ cstring p_name);
static void exprNode_checkUse (exprNode p_e, sRef p_s, fileloc p_loc);
static void exprNode_mergeUSs (exprNode p_res, exprNode p_other);
static void exprNode_mergeCondUSs (exprNode p_res, exprNode p_other1, exprNode p_other2);
static /*@only@*/ /*@notnull@*/ exprNode exprNode_fromIdentifierAux (/*@observer@*/ uentry p_c);
static void checkAnyCall (/*@notnull@*/ /*@dependent@*/ exprNode p_fcn, 
			  /*@dependent@*/ cstring p_fname,
			  uentryList p_pn, exprNodeList p_args, 
			  bool p_hasMods, sRefSet p_mods, bool p_isSpec,
			  int p_specialArgs);
static void checkOneArg (uentry p_ucurrent, /*@notnull@*/ exprNode p_current, 
			 /*@dependent@*/ exprNode p_fcn, bool p_isSpec, int p_argno, int p_totargs);
static void 
  checkUnspecCall (/*@notnull@*/ /*@dependent@*/ exprNode p_fcn, uentryList p_params, exprNodeList p_args);

static /*@only@*/ exprNode exprNode_effect (exprNode p_e)
  /*@globals internalState@*/ ;
static /*@only@*/ cstring exprNode_doUnparse (exprNode p_e);
static /*@observer@*/ cstring exprNode_rootVarName (exprNode p_e);
static /*@exposed@*/ exprNode 
  exprNode_lastStatement (/*@returned@*/ exprNode p_e);

static /*@null@*/ sRef defref = sRef_undefined;
static /*@only@*/ exprNode mustExitNode = exprNode_undefined;

static int checkArgsReal (uentry p_fcn, /*@dependent@*/ exprNode p_f, 
			  uentryList p_cl, 
			  exprNodeList p_args, bool p_isIter, exprNode p_ret);

static bool inEffect = FALSE;
static int nowalloc = 0;
static int totalloc = 0;
static int maxalloc = 0;

static /*@only@*/ uentry regArg;
static /*@only@*/ uentry outArg;
static /*@only@*/ uentry outStringArg;
static /*@exposed@*/ sRef stdinRef;
static /*@exposed@*/ sRef stdoutRef;
static /*@only@*/ uentry csArg;
static /*@only@*/ uentry csOnlyArg; 
static ctype cstringType;
static ctype ctypeType;
static ctype filelocType; 
static bool initMod = FALSE;

/*
** must occur after library has been read
*/

void exprNode_initMod (void)
  /*@globals undef regArg, undef outArg, undef outStringArg, 
             undef csOnlyArg, undef csArg; 
   @*/
{
  uentry ue;
  idDecl tmp;
  
  initMod = TRUE;
  cstringType = ctype_unknown;
  ctypeType = ctype_unknown;
  filelocType = ctype_unknown;

  defref = sRef_undefined;
  
  if (usymtab_existsType (cstring_makeLiteralTemp ("cstring")))
    {
      cstringType = usymtab_lookupAbstractType (cstring_makeLiteralTemp ("cstring"));
    }
 
  if (usymtab_existsType (cstring_makeLiteralTemp ("ctype")))
    {
      ctypeType = usymtab_lookupAbstractType (cstring_makeLiteralTemp ("ctype"));
    }

  if (usymtab_existsType (cstring_makeLiteralTemp ("fileloc")))
    {
      filelocType = usymtab_lookupAbstractType (cstring_makeLiteralTemp ("fileloc"));
    }

  if (usymtab_existsGlob (cstring_makeLiteralTemp ("stdin")))
    {
      ue = usymtab_lookupGlob (cstring_makeLiteralTemp ("stdin"));
    }
  else /* define stdin */
    {
      ue = uentry_makeVariable (cstring_makeLiteralTemp ("stdin"), 
				ctype_unknown, 
				fileloc_getBuiltin (), 
				FALSE);
      uentry_setHasNameError (ue); 
      ue = usymtab_supGlobalEntryReturn (ue);
    }

  stdinRef = sRef_makePointer (uentry_getSref (ue));
  
  if (usymtab_existsGlob (cstring_makeLiteralTemp ("stdout")))
    {
      ue = usymtab_lookupGlob (cstring_makeLiteralTemp ("stdout"));
    }
  else
    {
      ue = uentry_makeVariable (cstring_makeLiteralTemp ("stdout"), 
				ctype_unknown, 
				fileloc_getBuiltin (), 
				FALSE);
      uentry_setHasNameError (ue); 
      ue = usymtab_supGlobalEntryReturn (ue);
    }
  
  stdoutRef = sRef_makePointer (uentry_getSref (ue));

  tmp = idDecl_create (cstring_undefined, qtype_create (ctype_unknown));

  regArg = uentry_makeParam (tmp, PARAMUNKNOWN);

  idDecl_setTyp (tmp, 
		 qtype_addQual (qtype_create (ctype_makePointer (ctype_unknown)),
				qual_createOut ()));

  outArg = uentry_makeParam (tmp, PARAMUNKNOWN);

  idDecl_setTyp (tmp, qtype_addQual (qtype_create (ctype_string), 
				     qual_createOut ()));
  
  outStringArg = uentry_makeParam (tmp, PARAMUNKNOWN);
  
  idDecl_setTyp (tmp, qtype_addQual (qtype_addQual (qtype_create (cstringType), 
						    qual_createOnly ()),
				     qual_createNull ()));
  
  csOnlyArg = uentry_makeParam (tmp, PARAMUNKNOWN);
  
  idDecl_setTyp (tmp, qtype_addQual (qtype_create (cstringType), qual_createNull ()));
  csArg = uentry_makeParam (tmp, PARAMUNKNOWN);
  
  idDecl_free (tmp);
}

void
exprNode_destroyMod (void) 
   /*@globals killed regArg, killed outArg, killed outStringArg,
	      killed mustExitNode, initMod @*/
{
  if (initMod)
    {
      uentry_free (regArg);
      uentry_free (outArg);
      uentry_free (outStringArg);
      
      exprNode_free (mustExitNode);
      initMod = FALSE;
    /*@-branchstate@*/ 
    } 
  /*@=branchstate@*/
}

static void exprNode_resetSref (/*@notnull@*/ exprNode e)
{
  e->sref = defref;
}

static exprNode exprNode_fakeCopy (exprNode e)
{
  /*@-temptrans@*/ /*@-retalias@*/
  return e;
  /*@=temptrans@*/ /*@=retalias@*/
}

static bool isFlagKey (char key)
{
  return (key == '-' || key == '+' || key == ' ' || key == '#');
}

static void exprNode_combineControl (/*@notnull@*/ exprNode ret,
				     /*@notnull@*/ exprNode ifclause,
				     /*@notnull@*/ exprNode elseclause)
{
  ret->canBreak = ifclause->canBreak || elseclause->canBreak;

  ret->mustBreak =
    (ifclause->mustBreak || exprNode_mustEscape (ifclause))
      && (elseclause->mustBreak || exprNode_mustEscape (elseclause));

  ret->exitCode = exitkind_combine (ifclause->exitCode,
				    elseclause->exitCode);

}

/*
** For exprNode's returned by exprNode_effect.
*/

static bool shallowKind (exprKind kind)
{
  return (kind == XPR_STRINGLITERAL
	  || kind == XPR_NUMLIT
	  || kind == XPR_EMPTY
	  || kind == XPR_BODY
	  || kind == XPR_NODE);
}

static void 
exprNode_freeIniter (/*@only@*/ exprNode e)
{
  if (!exprNode_isError (e))
    {
      switch (e->kind)
	{
	case XPR_FACCESS:
	  sfree (e->edata->field);
	  sfree (e->edata);
	  break;
	case XPR_FETCH:
	  exprNode_free (e->edata->op->b);
	  /*@-compdestroy@*/ sfree (e->edata->op); /*@=compdestroy@*/
	  sfree (e->edata);
	  break;
	default:
	  llbug (message ("other: %s", exprNode_unparse (e)));
	}

      multiVal_free (e->val);
      cstring_free (e->etext);
      fileloc_free (e->loc);
      sRefSet_free (e->uses);
      sRefSet_free (e->sets);
      sRefSet_free (e->msets);
      guardSet_free (e->guards);
      sfree (e);
    }
}

void 
exprNode_freeShallow (/*@only@*/ exprNode e)
{
  if (!exprNode_isError (e))
    {
      if (shallowKind (e->kind))
	{
	  	}
      else
	{
	  if (!inEffect)
	    {
	      if (e->kind == XPR_EMPTY
		  || e->kind == XPR_BODY
		  || e->kind == XPR_STRINGLITERAL
		  || e->kind == XPR_NUMLIT
		  || e->kind == XPR_NODE
		  || e->kind == XPR_OFFSETOF
		  || e->kind == XPR_ALIGNOFT
		  || e->kind == XPR_ALIGNOF
		  || e->kind == XPR_SIZEOFT
		  || e->kind == XPR_SIZEOF)
		{
		  /* don't free anything */
		}
	      else
		{
		  /* multiVal_free (e->val);  */
		  cstring_free (e->etext);
		  fileloc_free (e->loc);
		  sRefSet_free (e->uses);
		  sRefSet_free (e->sets);
		  sRefSet_free (e->msets);
		  guardSet_free (e->guards);
		  exprData_freeShallow (e->edata, e->kind); 
		  nowalloc--;
		  /*@-compdestroy@*/ sfree (e); /*@=compdestroy@*/
		  /*@-branchstate@*/
		}
	    }
	} /*@=branchstate@*/
    }
  }

void
exprNode_free (exprNode e)
{
  if (!exprNode_isError (e))
    {
      if (!inEffect)
	{
	  multiVal_free (e->val);
	  cstring_free (e->etext);
	  fileloc_free (e->loc);
	  sRefSet_free (e->uses);
	  sRefSet_free (e->sets);
	  sRefSet_free (e->msets);
	  guardSet_free (e->guards);
	  exprData_free (e->edata, e->kind);
	  
	  nowalloc--;
	  sfree (e);
	  /*@-branchstate@*/ 
	} /*@=branchstate@*/
    }
}

exprNode
exprNode_makeError ()
{
  return exprNode_undefined;
}

static /*@out@*/ /*@only@*/ /*@notnull@*/ exprNode
exprNode_new (void)
{
  exprNode ret = (exprNode) dmalloc (sizeof (*ret));
  /* static int lastexpnodes = 0; */

  nowalloc++;
  totalloc++;

  if (nowalloc > maxalloc)
    {
      maxalloc = nowalloc;
    }

  return ret;
}

static /*@notnull@*/ /*@special@*/ exprNode
  exprNode_createPlain (ctype c) 
  /*@defines result@*/
  /*@post:isnull result->edata, result->loc, result->val, result->guards,
                 result->uses, result->sets, result->msets, result->etext @*/
  /*@*/
{
  exprNode e = exprNode_new ();

  e->typ = c;
  e->kind = XPR_EMPTY;
  e->val = multiVal_undefined;
  e->sref = defref;
  e->etext = cstring_undefined;
  e->loc = fileloc_undefined;
  e->guards = guardSet_undefined;
  e->uses = sRefSet_undefined;
  e->sets = sRefSet_undefined;
  e->msets = sRefSet_undefined;
  e->edata = exprData_undefined;
  e->exitCode = XK_NEVERESCAPE;
  e->canBreak = FALSE;
  e->mustBreak = FALSE;
  e->isJumpPoint = FALSE;
  return (e);
}

/*@observer@*/ exprNode exprNode_makeMustExit (void)
{
  if (exprNode_isUndefined (mustExitNode))
    {
      mustExitNode = exprNode_createPlain (ctype_unknown);
      mustExitNode->exitCode = XK_MUSTEXIT;
    }

  return mustExitNode;
}


static /*@notnull@*/ /*@special@*/ exprNode exprNode_create (ctype c)
  /*@defines result@*/
  /*@post:isnull result->edata, result->guards, result->val,
                 result->uses, result->sets, result->msets@*/
  /*@*/
{
  exprNode e = exprNode_createPlain (c);
  e->loc = fileloc_copy (g_currentloc);
  return (e);
}

static /*@notnull@*/ /*@special@*/ exprNode exprNode_createUnknown (void)
  /*@defines result@*/
  /*@post:isnull result->edata, result->guards,
                 result->uses, result->sets, result->msets@*/
  /*@*/
{
  return (exprNode_create (ctype_unknown));
}

static /*@notnull@*/ /*@special@*/ exprNode
  exprNode_createLoc (ctype c, /*@keep@*/ fileloc loc)
  /*@defines result@*/
  /*@post:isnull result->edata, result->guards, result->val,
                 result->uses, result->sets, result->msets@*/
  /*@*/
{
  exprNode e = exprNode_createPlain (c);
  e->loc = loc;
  return (e);
}

static void 
  exprNode_copySets (/*@special@*/ /*@notnull@*/ exprNode ret, exprNode e)
  /*@defines ret->guards, ret->uses, ret->sets, ret->msets@*/
{
  if (exprNode_isDefined (e))
    {
      ret->guards = guardSet_copy (e->guards);
      ret->uses = sRefSet_newCopy (e->uses);
      ret->sets = sRefSet_newCopy (e->sets);
      ret->msets = sRefSet_newCopy (e->msets); 
    }
  else
    {
      ret->guards = guardSet_undefined;
      ret->uses = sRefSet_undefined;
      ret->sets = sRefSet_undefined;
      ret->msets = sRefSet_undefined;
    }
}

static /*@notnull@*/ /*@special@*/ exprNode
  exprNode_createPartialLocCopy (exprNode e, /*@only@*/ fileloc loc)
  /*@defines result@*/
  /*@post:isnull result->edata, result->etext@*/
  /*@*/
{
  exprNode ret = exprNode_new ();

  if (exprNode_isError (e))
    {
      ret->typ = ctype_unknown;
      ret->val = multiVal_undefined;
      ret->loc = loc;
      ret->guards = guardSet_undefined;
      ret->uses = sRefSet_undefined;
      ret->sets = sRefSet_undefined;
      ret->msets = sRefSet_undefined;
    }
  else
    {
      ret->typ = e->typ;
      ret->val = multiVal_copy (e->val);
      ret->loc = loc;
      ret->guards = guardSet_copy (e->guards);
      ret->uses = sRefSet_newCopy (e->uses);
      ret->sets = sRefSet_newCopy (e->sets);
      ret->msets = sRefSet_newCopy (e->msets); 
    }

  ret->kind = XPR_EMPTY;
  ret->sref = defref;
  ret->etext = cstring_undefined;
  ret->exitCode = XK_NEVERESCAPE;
  ret->canBreak = FALSE;
  ret->mustBreak = FALSE;
  ret->isJumpPoint = FALSE;
  ret->edata = exprData_undefined;

  return (ret);
}


static /*@notnull@*/ /*@special@*/ exprNode
  exprNode_createPartialCopy (exprNode e)
  /*@defines result@*/
  /*@post:isnull result->edata, result->etext@*/
  /*@*/
{
  return (exprNode_createPartialLocCopy (e, fileloc_copy (exprNode_loc (e))));
}

static /*@notnull@*/ /*@special@*/ exprNode
  exprNode_createPartialNVCopy (exprNode e)
  /*@defines result@*/
  /*@post:isnull result->edata, result->etext, result->val @*/
  /*@*/
{
  exprNode ret = exprNode_new ();

  if (exprNode_isError (e))
    {
      ret->typ = ctype_unknown;
      ret->loc = fileloc_undefined;
      ret->guards = guardSet_undefined;
      ret->uses = sRefSet_undefined;
      ret->sets = sRefSet_undefined;
      ret->msets = sRefSet_undefined;
    }
  else
    {
      ret->typ = e->typ;
      ret->loc = fileloc_copy (e->loc);
      ret->guards = guardSet_copy (e->guards);
      ret->uses = sRefSet_newCopy (e->uses);
      ret->sets = sRefSet_newCopy (e->sets);
      ret->msets = sRefSet_newCopy (e->msets); 
    }
  
  ret->val = multiVal_undefined;
  ret->kind = XPR_EMPTY;
  ret->sref = defref;
  ret->etext = cstring_undefined;
  ret->exitCode = XK_NEVERESCAPE;
  ret->canBreak = FALSE;
  ret->mustBreak = FALSE;
  ret->isJumpPoint = FALSE;
  ret->edata = exprData_undefined;

  return (ret);
}

static /*@notnull@*/ /*@special@*/ exprNode
  exprNode_createSemiCopy (exprNode e)
  /*@defines result@*/
  /*@post:isnull result->edata, result->etext, result->sets,
                 result->msets, result->uses, result->guards@*/
  /*@*/
{
  if (exprNode_isError (e))
    {
      return exprNode_createPlain (ctype_unknown);
    }
  else
    {
      exprNode ret = exprNode_new ();

      ret->typ = e->typ;
      ret->val = multiVal_copy (e->val);
      ret->loc = fileloc_copy (e->loc);
      ret->guards = guardSet_undefined;
      ret->uses = sRefSet_undefined;
      ret->sets = sRefSet_undefined;
      ret->msets = sRefSet_undefined;

      ret->kind = XPR_EMPTY;
      ret->sref = defref;
      ret->etext = cstring_undefined;
      ret->exitCode = XK_NEVERESCAPE;
      ret->canBreak = FALSE;
      ret->mustBreak = FALSE;
      ret->isJumpPoint = FALSE;
      ret->edata = exprData_undefined;
      
      return (ret);
    }
}

bool
exprNode_isNullValue (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      multiVal m = exprNode_getValue (e);
      
      if (multiVal_isInt (m))
	{
	  return (multiVal_forceInt (m) == 0);
	}
    }
  
  return FALSE;
}

static bool
exprNode_isUnknownConstant (/*@notnull@*/ exprNode e)
{
  while (e->kind == XPR_PARENS)
    {
      e = exprData_getUopNode (e->edata);
      llassert (exprNode_isDefined (e));
    }

  if (e->kind == XPR_CONST)
    {
      multiVal m = exprNode_getValue (e);

      if (multiVal_isUnknown (m)) 
	{
	  return TRUE;
	}
    }
  
  return FALSE;
}

/*@only@*/ exprNode
  exprNode_numLiteral (ctype c, /*@temp@*/ cstring t, 
		       /*@only@*/ fileloc loc, long val)
{
  exprNode e = exprNode_createLoc (c, loc);

  e->kind = XPR_NUMLIT;
  
  llassert (multiVal_isUndefined (e->val));
  e->val = multiVal_makeInt (val);
  e->edata = exprData_makeLiteral (cstring_copy (t));

  if (val == 0)
    {
      e->sref = sRef_makeUnknown ();
      sRef_setDefNull (e->sref, e->loc);
    }

  DPRINTF (("Num lit: %s / %s", exprNode_unparse (e), ctype_unparse (exprNode_getType (e))));
  return (e);
}

/*@only@*/ exprNode
exprNode_charLiteral (char c, cstring text, /*@only@*/ fileloc loc)
{
  exprNode e = exprNode_createLoc (ctype_char, loc);

  if (context_getFlag (FLG_CHARINTLITERAL))
    {
      e->typ = ctype_makeConj (ctype_char, ctype_int);
    }

  e->kind = XPR_NUMLIT;
  e->val = multiVal_makeChar (c);

  e->edata = exprData_makeLiteral (cstring_copy (text));
  return (e);
}

/*@only@*/ exprNode
exprNode_floatLiteral (double d, ctype ct, cstring text, /*@only@*/ fileloc loc)
{
  exprNode e = exprNode_createLoc (ct, loc);

  e->kind = XPR_NUMLIT;
    e->val = multiVal_makeDouble (d);
  e->edata = exprData_makeLiteral (cstring_copy (text));
  return (e);
}

multiVal exprNode_getValue (exprNode e) 
{
  while (exprNode_isInParens (e)) {
    if (e->edata != NULL) {
      e = exprData_getUopNode (e->edata);
    } else {
      break;
    }
  }

  if (exprNode_isDefined (e)) {
    return e->val; 
  } else {
    return multiVal_undefined;
  }
}

/*@only@*/ exprNode
exprNode_stringLiteral (/*@only@*/ cstring t, /*@only@*/ fileloc loc)
{
  exprNode e = exprNode_createLoc (ctype_string, loc);
  int len = cstring_length (t) - 2;
  char *ts = cstring_toCharsSafe (t);
  char *s = cstring_toCharsSafe (cstring_create (len + 1));

  if (context_getFlag (FLG_STRINGLITERALLEN))
    {
      if (len > context_getValue (FLG_STRINGLITERALLEN))
	{
	  voptgenerror (FLG_STRINGLITERALLEN,
			message
			("String literal length (%d) exceeds maximum "
			 "length (%d): %s",
			 len,
			 context_getValue (FLG_STRINGLITERALLEN),
			 t),
			e->loc);
	}
    }

  strncpy (s, ts+1, size_fromInt (len));
  *(s + len) = '\0';

  
  e->kind = XPR_STRINGLITERAL;
  e->val = multiVal_makeString (cstring_fromCharsO (s));
  e->edata = exprData_makeLiteral (t);
  e->sref = sRef_makeType (ctype_string);

  if (context_getFlag (FLG_READONLYSTRINGS))
    {
      sRef_setAliasKind (e->sref, AK_STATIC, fileloc_undefined);
      sRef_setExKind (e->sref, XO_OBSERVER, loc);
    }
  else
    {
      sRef_setAliasKind (e->sref, AK_ERROR, fileloc_undefined);
    }

  return (e); /* s released */
}

exprNode exprNode_fromUIO (cstring c)
{
  fileloc loc = context_getSaveLocation ();
  exprNode e  = exprNode_createPlain (ctype_unknown);

  e->kind = XPR_VAR;

  if (fileloc_isUndefined (loc))
    {
      loc = fileloc_copy (g_currentloc);
    }

  e->loc = loc; /* save loc was mangled */
  e->sref = defref;

  if (usymtab_exists (c))
    {
      uentry ue = usymtab_lookupEither (c);

      if (uentry_isDatatype (ue) 
	  && uentry_isSpecified (ue))
	{
	  llfatalerror
	    (message ("%q: Specified datatype %s used in code, but not defined. "
		      "(Cannot continue reasonably from this error.)",
		      fileloc_unparse (e->loc), c));
	}
      else
	{
	  BADBRANCH; 
	}
    }
  
  llassertprint (!usymtab_exists (c), ("Entry exists: %s", c));

  /*
  ** was supercedeGlobalEntry...is this better?
  */

  if (!context_inIterEnd ())
    {
      if (context_inMacro ())
        {
	  if (context_getFlag (FLG_UNRECOG))
	    {
	      voptgenerror 
		(FLG_MACROUNDEF, 
		 message ("Unrecognized identifier in macro definition: %s", c), e->loc);
	    }
	  else
	    {
	      flagcode_recordSuppressed (FLG_UNRECOG);
	    }
        }
      else
        {
          voptgenerror 
	    (FLG_UNRECOG, message ("Unrecognized identifier: %s", c),
	     e->loc);
	}
    }
  
  e->edata = exprData_makeId (uentry_makeUnrecognized (c, fileloc_copy (loc)));

  /* No alias errors for unrecognized identifiers */
  sRef_setAliasKind (e->sref, AK_ERROR, loc); 

  return (e);
}

exprNode exprNode_createId (/*@observer@*/ uentry c)
{
  if (uentry_isValid (c))
    {
      exprNode e = exprNode_new ();
      
      e->typ = uentry_getType (c);

      if (uentry_isFunction (c)
	  && !sRef_isLocalVar (uentry_getSref (c)))
	{
	  e->sref = sRef_undefined;
	}
      else
	{
	  e->sref = uentry_getSref (c);	
	}

      if (sRef_isStateUnknown (e->sref) && uentry_isNonLocal (c))
	{
	  sRef_setDefined (e->sref, fileloc_undefined);
	}
      
      /*
      ** yoikes!  leaving this out was a heinous bug...that would have been
      ** caught if i had lclint working first.  gag!
      */
      
      e->etext = cstring_undefined;
      
      if (uentry_isEitherConstant (c))
	{
	  e->kind = XPR_CONST;
	  e->val = multiVal_copy (uentry_getConstantValue (c));
	}
      else
	{
	  e->kind = XPR_VAR;
	  e->val = multiVal_unknown ();
	}
      
      e->edata = exprData_makeId (c);
      e->loc = context_getSaveLocation ();
      
      if (fileloc_isUndefined (e->loc))
	{
	  fileloc_free (e->loc);
	  e->loc = fileloc_copy (g_currentloc);
	}

      e->guards = guardSet_new ();
      e->sets = sRefSet_new ();
      e->msets = sRefSet_new ();
      e->uses = sRefSet_new ();
      
      /*> missing fields, detected by lclint <*/
      e->exitCode = XK_NEVERESCAPE;
      e->isJumpPoint = FALSE;
      e->canBreak = FALSE;
      e->mustBreak = FALSE;
      
      return e;
    }
  else
    {
            return exprNode_createUnknown ();
    }
}

/*@notnull@*/ exprNode
exprNode_fromIdentifier (/*@observer@*/ uentry c)
{
  exprNode ret;

  if (context_justPopped ()) /* watch out! c could be dead */
    { 
      uentry ce = usymtab_lookupSafe (LastIdentifier ());

      if (uentry_isValid (ce)) 
        {
          c = ce;
        }
      else
	{
	  llbuglit ("Looks like Aunt Millie forgot to walk to dog again.");
	}
    }

  ret = exprNode_fromIdentifierAux (c);
  
  return ret;
}


static /*@only@*/ /*@notnull@*/ exprNode
exprNode_fromIdentifierAux (/*@observer@*/ uentry c)
{
  exprNode e = exprNode_createId (c);
  sRef sr = e->sref;

  uentry_setUsed (c, e->loc);

  if (uentry_isVar (c) && sRef_isGlobal (sr))
    {
      checkGlobUse (c, FALSE, e);
    }

  return (e);
}

static bool
exprNode_isZero (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      multiVal m = exprNode_getValue (e);
      
      if (multiVal_isInt (m))
	{
	  return (multiVal_forceInt (m) == 0);
	}
    }

  return FALSE;
}

static bool
exprNode_isNonNegative (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      multiVal m = exprNode_getValue (e);
      
      if (multiVal_isInt (m))
	{
	  return (multiVal_forceInt (m) >= 0);
	}
    }

  return FALSE;
}

/*
** a[x]  - uses a but NOT a[] 
**         result sref = a[]  (set/use in assignment)
** 
** The syntax x[a] is also legal in C, and has the same 
** semantics.  If ind is an array, and arr is an int, flip
** the arguments.
*/

/*@only@*/ exprNode
exprNode_arrayFetch (/*@only@*/ exprNode e1, /*@only@*/ exprNode e2)
{
  /*
  ** error in arr, error propagates (no new messages)
  ** error in ind, assume valid and continue
  */

  if (exprNode_isError (e1))
    {
      exprNode_free (e2);
      return (exprNode_makeError ());
    }
  else
    {
      exprNode arr;
      exprNode ind;
      ctype carr = exprNode_getType (e1);
      ctype crarr = ctype_realType (carr);
 
      /*
      ** this sets up funny aliasing, that leads to spurious
      ** lclint errors.  Hence, the i2 comments.
      */

      if (!ctype_isRealArray (crarr) 
	  && ctype_isRealNumeric (crarr) 
	  && !exprNode_isError (e2)
	  && ctype_isRealAP (exprNode_getType (e2)))  /* fetch like 3[a] */
	{
	  arr = e2;
	  ind = e1;

	  carr = exprNode_getType (arr);
	  crarr = ctype_realType (carr);
	}
      else
	{
	  arr = e1;
	  ind = e2;
	}

      if (sRef_possiblyNull (arr->sref))
        {
          if (!usymtab_isGuarded (arr->sref))
            {
              if (optgenerror (FLG_NULLDEREF,
			       message ("Index of %s pointer %q: %s", 
					sRef_nullMessage (arr->sref),
					sRef_unparse (arr->sref),
					exprNode_unparse (arr)),
			       arr->loc))
		{
		  sRef_showNullInfo (arr->sref);

		  /* suppress future messages */
		  sRef_setNullError (arr->sref); 
		}
            }
        }

      if (exprNode_isError (ind))
        {
          if ((ctype_isArrayPtr (crarr) 
	       && !ctype_isFunction (crarr))
	      || ctype_isUnknown (carr))
            {
              exprNode ret = exprNode_createPartialCopy (arr);

              if (ctype_isKnown (carr))
		{
		  ret->typ = ctype_baseArrayPtr (crarr);
		}
              else
		{
		  ret->typ = ctype_unknown;
		}
	      
              ret->sref = sRef_makeArrayFetch (arr->sref);
	      
              ret->kind = XPR_FETCH;

	      /*
              ** Because of funny aliasing (when arr and ind are
	      ** flipped) spurious errors would be reported here.
	      */
	 
              /*@i2@*/ ret->edata = exprData_makePair (arr, ind);	      
              checkSafeUse (ret, arr->sref);
              return (ret);
            }
          else
            {
              voptgenerror (FLG_TYPE,
			    message ("Array fetch from non-array (%t): %s[%s]", carr, 
				     exprNode_unparse (e1), exprNode_unparse (e2)),
			    arr->loc);
              exprNode_free (arr);
              return (exprNode_makeError ());
            }
        }
      else
        {
          if (!ctype_isForceRealInt (&(ind->typ)))
            {
	      ctype rt = ctype_realType (ind->typ);

              if (ctype_isChar (rt))
		{
		  vnoptgenerror
		    (FLG_CHARINDEX,
		     message ("Array fetch using non-integer, %t: %s[%s]",
			      ind->typ, 
			      exprNode_unparse (e1), exprNode_unparse (e2)),
		     arr->loc);
		}
	      else if (ctype_isEnum (rt))
		{
		  vnoptgenerror
		    (FLG_ENUMINDEX,
		     message ("Array fetch using non-integer, %t: %s[%s]",
			      ind->typ, 
			      exprNode_unparse (e1), exprNode_unparse (e2)),
		     arr->loc);
		}
              else
		{
		  voptgenerror 
		    (FLG_TYPE,
		     message ("Array fetch using non-integer, %t: %s[%s]",
			      ind->typ, 
			      exprNode_unparse (e1), exprNode_unparse (e2)),
		     arr->loc);
		}

	      multiVal_free (ind->val);
              ind->val = multiVal_unknown ();
            }
	  
          if (ctype_isArrayPtr (crarr) && !ctype_isFunction (crarr))
            {
              exprNode ret = exprNode_createSemiCopy (arr);
              multiVal m = exprNode_getValue (ind);
	      
              ret->typ = ctype_baseArrayPtr (crarr);
              ret->kind = XPR_FETCH;
	      
              if (multiVal_isInt (m))
		{
		  int i = (int) multiVal_forceInt (m);
		  
		  if (sRef_isValid (arr->sref)) {
		    ret->sref = sRef_makeArrayFetchKnown (arr->sref, i);
		  } else {
		    ret->sref = sRef_undefined;
		  }
		}
              else
		{
		  		  ret->sref = sRef_makeArrayFetch (arr->sref);
		}
	      
              ret->sets = sRefSet_realNewUnion (arr->sets, ind->sets);
              ret->msets = sRefSet_realNewUnion (arr->msets, ind->msets);
              ret->uses = sRefSet_realNewUnion (arr->uses, ind->uses);
	      
	      /* (see comment on spurious errors above) */
              /*@i2@*/ ret->edata = exprData_makePair (arr, ind);
	      
              exprNode_checkUse (ret, ind->sref, ind->loc);
              exprNode_checkUse (ret, arr->sref, arr->loc);
	      
              return (ret);
            }
          else
            {
              if (ctype_isUnknown (carr))
		{
		  exprNode ret = exprNode_createPartialCopy (arr);
		  
		  ret->kind = XPR_FETCH;
		  ret->typ = ctype_unknown;
		  ret->sets = sRefSet_union (ret->sets, ind->sets);
		  ret->msets = sRefSet_union (ret->msets, ind->msets);
		  ret->uses = sRefSet_union (ret->uses, ind->uses);

		  /* (see comment on spurious errors above) */		  
		  /*@i2@*/ ret->edata = exprData_makePair (arr, ind);
		  
		  exprNode_checkUse (ret, ind->sref, ind->loc);
		  exprNode_checkUse (ret, arr->sref, arr->loc);
		  return (ret);
		}
              else
		{
		  voptgenerror
		    (FLG_TYPE,
		     message ("Array fetch from non-array (%t): %s[%s]", carr, 
			      exprNode_unparse (e1), exprNode_unparse (e2)),
		     arr->loc);

		  exprNode_free (arr);
		  exprNode_free (ind);

		  return (exprNode_makeError ());
		}
            }
        }
    }
  BADEXIT;
}


static int
checkArgs (uentry fcn, /*@dependent@*/ exprNode f, ctype t, 
	   exprNodeList args, exprNode ret)
{
  return (checkArgsReal (fcn, f, ctype_argsFunction (t), args, FALSE, ret));
}

/*
** checkPrintfArgs --- checks arguments for printf-like functions
**    Arguments before ... have already been checked.
**    The argument before the ... is a char *.  
**    argno is the format string argument.
*/

static void
checkPrintfArgs (/*@notnull@*/ /*@dependent@*/ exprNode f, uentry fcn, 
		 exprNodeList args, exprNode ret, int argno)
{
  /*
  ** the last argument before the elips is the format string 
  */

  int i = argno;
  fileloc formatloc;
  int nargs = exprNodeList_size (args);
  uentryList params = uentry_getParams (fcn);
  exprNode a; 

  /*
  ** These should be ensured by checkSpecialFunction
  */

  llassert (uentryList_size (params) == argno + 1);
  llassert (uentry_isElipsisMarker (uentryList_getN (params, argno)));

  a = exprNodeList_getN (args, argno - 1);
  formatloc = fileloc_copy (exprNode_loc (a));

  if (exprNode_isDefined (a) && exprNode_isStringLiteral (a) 
      && exprNode_knownStringValue (a))
    {
      char *format = cstring_toCharsSafe (multiVal_forceString (exprNode_getValue (a)));
      char *code = format;
      char *ocode = code;

      nargs = exprNodeList_size (args);

      while ((code = strchr (code, '%')) != NULL)
        {
          char *origcode = code;
          char key = *(++code);			
          ctype modtype = ctype_int;
          bool modified = FALSE;

	  fileloc_addColumn (formatloc, code - ocode);

	  /* ignore flags */
	  while (isFlagKey (key)) 
	    {
	      key = *(++code);
	      fileloc_incColumn (formatloc);
	    }

	  if (key == 'm') /* skipped in syslog */
	    {
	      continue; 
	    }
	  
	  /* ignore field width */
	  while (isdigit ((int) key) != 0) 
	    {
	      key = *(++code);
	      fileloc_incColumn (formatloc);
	    }
	  
	  /* ignore precision */
	  if (key == '.')
	    {
	      key = *(++code);
	      fileloc_incColumn (formatloc);

	      /*
	      ** In printf, '*' means: read the next arg as an int for the
	      ** field width.  This seems to be missing from my copy of the 
	      ** standard x3.159-1989.  Setion 4.9.6.1 refers to * (described
	      ** later) but never does.
	      */
	      
	      if (key == '*') 
		{
		  ; /* don't do anything --- handle later */
		}
	      else
		{
		  while (isdigit ((int) key) != 0)
		    {
		      key = *(++code);
		      fileloc_incColumn (formatloc);
		    }
		}
	    }

          if (key == 'h')
            {
              modtype = ctype_sint;  /* short */
              key = *(++code);
	      fileloc_incColumn (formatloc);
            }
          else if (key == 'l' || key == 'L') 
            {
              modtype = ctype_lint; /* long */
              key = *(++code);
	      fileloc_incColumn (formatloc);
            }
	  else
	    {
	      ; /* no modifier */
	    }
	  
          /* now, key = type of conversion to apply */
          ++code;
	  fileloc_incColumn (formatloc);

          if (key != '%') 
            {
	      if (i >= nargs)
		{
		  if (optgenerror 
		      (FLG_TYPE,
		       message ("No argument corresponding to %q format "
				"code %d (%%%h): \"%s\"",
				uentry_getName (fcn),
				i, key,
				cstring_fromChars (format)),
		       f->loc))
		    {
		      if (fileloc_isDefined (formatloc)
			  && context_getFlag (FLG_SHOWCOL))
			{
			  llgenindentmsg (cstring_makeLiteral ("Corresponding format code"),
					  formatloc);
			}
		    }
		  i++;
		}
	      else
		{
		  a = exprNodeList_getN (args, i);
		  i++;
		  
		  if (!exprNode_isError (a))
		    {
		      ctype expecttype;

		      switch (key)
			{
			case '*': /* int argument for fieldwidth */
			  expecttype = ctype_int;
			  *(--code) = '%'; /* convert it for next code */
			  fileloc_subColumn (formatloc, 1);
			  /*@switchbreak@*/ break;		
			case 'u':
			case 'o':
			  expecttype = ctype_combine (ctype_uint, modtype);
			  /*@switchbreak@*/ break;
			  
			case 'i': /* int argument */ 
			case 'd':
			  expecttype = ctype_combine (ctype_int, modtype);
			  /*@switchbreak@*/ break;

			case 'x': /* unsigned int */
			case 'X':
			  expecttype = ctype_combine (ctype_uint, modtype);
			  /*@switchbreak@*/ break;
			  
			case 'e':
			case 'E':
			case 'g':
			case 'G':
			case 'f': /* double */
			  expecttype = ctype_combine (ctype_double, modtype);
			  /*@switchbreak@*/ break;
			  
			case 'c': /* int converted to char (check its a char?) */
			  expecttype = ctype_makeConj (ctype_char, ctype_uchar);
			  /*@switchbreak@*/ break;
			      
			case 's': /* string */
			  expecttype = ctype_string;
			  /*@switchbreak@*/ break;
			case '[': 
			  /* skip to ']' */
			  while (((key = *(++code)) != ']') 
				 && (key != '\0'))
			    {
			      fileloc_incColumn (formatloc);
			    }
			  
			  if (key == '\0')
			    {
			      llfatalerrorLoc
				(message ("Bad character set format: %s", 
					  cstring_fromChars (origcode)));
			    }
			  
			  expecttype = ctype_string;
			  /*@switchbreak@*/ break;
			  
			case 'p': /* pointer */
			  expecttype = ctype_makePointer (ctype_void);
			  /* really! */
			  /*@switchbreak@*/ break;
			  
			case 'n': /* pointer to int, modified by call! */
			  expecttype = ctype_combine (ctype_makePointer (ctype_int), modtype);
			  modified = TRUE;
			  uentry_setDefState (regArg, SS_ALLOCATED); /* corresponds to out */
			  /*@switchbreak@*/ break;

			case 'm': /* in a syslog, it doesn't consume an argument */
			  /* should check we're really doing syslog */
			  
			  /*@switchbreak@*/ break;

			  
			default:
			  expecttype = ctype_unknown;
			  
			  voptgenerror
			    (FLG_FORMATCODE,
			     message ("Unrecognized format code: %s", 
				      cstring_fromChars (origcode)),
			     fileloc_isDefined (formatloc) 
			     ? formatloc : g_currentloc);

			  /*@switchbreak@*/ break;
			}

		      if (!(exprNode_matchArgType (expecttype, a)))
			{
			  if (ctype_isVoidPointer (expecttype) 
			      && ctype_isRealAbstract (a->typ)
			      && (context_getFlag (FLG_ABSTVOIDP)))
			    {
			      ;
			    }
			  else
			    {
			      if (llgenformattypeerror 
				  (expecttype, exprNode_undefined,
				   a->typ, a,
				   message ("Format argument %d to %q (%%%h) expects "
					    "%t gets %t: %s",
					    i - argno,
					    uentry_getName (fcn), 
					    key, expecttype,
					    a->typ, exprNode_unparse (a)),
				   a->loc))
				{
				  if (fileloc_isDefined (formatloc)
				      && context_getFlag (FLG_SHOWCOL))
				    {
				      llgenindentmsg
					(cstring_makeLiteral 
					 ("Corresponding format code"),
					 formatloc);
				    }
				}
			    }
			}

		      uentry_setType (regArg, expecttype);
		      checkOneArg (regArg, a, f, FALSE, i+1, nargs);
		      
		      if (ctype_equal (expecttype, ctype_string))
			{
			  exprNode_checkUse (a, sRef_makePointer (a->sref), a->loc);
			}
		      
		      uentry_setType (regArg, ctype_unknown);
		      uentry_fixupSref (regArg);
		  
		      if (modified)
			{
			  exprNode_checkCallModifyVal (a->sref, args, f, ret);
			}
		    }
		  else
		    {
		      ;
		    }
		}
	    }
	  ocode = code;
	}
  
      if (i < nargs)
	{
	  voptgenerror (FLG_TYPE,
			message ("Format string for %q has %d arg%p, given %d", 
				 uentry_getName (fcn), i - argno, nargs - argno),
			f->loc);
	}
    }
  else
    {
      /* no checking possible for compile-time unknown format strings */
    }

  fileloc_free (formatloc);
}

static void
checkScanfArgs (/*@notnull@*/ /*@dependent@*/ exprNode f, uentry fcn, 
		 exprNodeList args, exprNode ret, int argno)
{
  int i = argno;
  fileloc formatloc;
  int nargs = exprNodeList_size (args);
  uentryList params = uentry_getParams (fcn);
  exprNode a; 

  /*
  ** These should be ensured by checkSpecialFunction
  */

  llassert (uentryList_size (params) == argno + 1);
  llassert (uentry_isElipsisMarker (uentryList_getN (params, argno)));

  a = exprNodeList_getN (args, argno - 1);
  formatloc = fileloc_copy (exprNode_loc (a));

  if (exprNode_isDefined (a) && exprNode_isStringLiteral (a) 
      && exprNode_knownStringValue (a))
    {
      char *format = cstring_toCharsSafe (multiVal_forceString (exprNode_getValue (a)));
      char *code = format;
      char *ocode = code;

      nargs = exprNodeList_size (args);

      while ((code = strchr (code, '%')) != NULL)
        {
          char *origcode = code;
          char key = *(++code);			
          ctype modtype = ctype_int;
	  char modifier = '\0';
          bool modified = TRUE;
	  bool ignore = FALSE;

	  fileloc_addColumn (formatloc, code - ocode);

	  /*
	  ** this is based on ANSI standard library description of fscanf
	  ** (from ANSI standard X3.159-1989, 4.9.6.1)
	  */
	      
	  /* '*' suppresses assignment (does not need match argument) */
	  
	  if (key == '*') 
	    {
	      key = *(++code);
	      modified = FALSE; 
	      ignore = TRUE;
	      fileloc_incColumn (formatloc);
	    }
	  
	  /* ignore field width */
	  while (isdigit ((int) key) != 0)
	    {
	      key = *(++code);
	      fileloc_incColumn (formatloc);
	    }
	  
          if (key == 'h')
            {
              modtype = ctype_sint;  /* short */
              key = *(++code);
	      fileloc_incColumn (formatloc);
            }
          else if (key == 'l' || key == 'L') 
            {
              modtype = ctype_lint; /* long */
	      modifier = key;

              key = *(++code);
	      fileloc_incColumn (formatloc);
            }
	  else
	    {
	      ; /* no modifier */
	    }
	  
          /* now, key = type of conversion to apply */
          ++code;
	  fileloc_incColumn (formatloc);

          if (key != '%') 
            {
	      if (ignore)
		{
		  ;
		}
	      else
		{
		  if (i >= nargs)
		    {
		      if (optgenerror 
			  (FLG_TYPE,
			   message ("No argument corresponding to %q format "
				    "code %d (%%%h): \"%s\"",
				    uentry_getName (fcn),
				    i, key,
				    cstring_fromChars (format)),
			   f->loc))
			{
			  if (fileloc_isDefined (formatloc)
			      && context_getFlag (FLG_SHOWCOL))
			    {
			      llgenindentmsg
				(cstring_makeLiteral ("Corresponding format code"),
				 formatloc);
			    }
			}
		      i++;
		    }
		  else
		    {
		      a = exprNodeList_getN (args, i);
		      i++;
		  
		      if (!exprNode_isError (a))
			{
			  ctype expecttype;

			  switch (key)
			    {
			    case '*': /* int argument for fieldwidth */
			      expecttype = ctype_makePointer (ctype_int);
			      *(--code) = '%'; /* convert it for next code */
			      fileloc_subColumn (formatloc, 1);
			      /*@switchbreak@*/ break;		
			    case 'u':
			    case 'o':
			      expecttype = ctype_makePointer (ctype_combine (ctype_uint, modtype));
			      /*@switchbreak@*/ break;
			      
			    case 'i': 
			    case 'd':
			    case 'x':
			    case 'X': /* unsigned int */
			      expecttype = ctype_makePointer (ctype_combine (ctype_int, modtype));
			      /*@switchbreak@*/ break;
			      
			    case 'e':
			    case 'E':
			    case 'g':
			    case 'G':
			    case 'f': 
			      /* printf is double, scanf is float! */

			      			      if (modifier == 'l') 
				{
				  expecttype = ctype_makePointer (ctype_double);
				}
			      else if (modifier == 'L')
				{
				  expecttype = ctype_makePointer (ctype_ldouble);
				}
			      else 
				{
				  llassert (modifier == '\0');
				  expecttype = ctype_makePointer (ctype_float);
				}
			      /*@switchbreak@*/ break;
			      
			    case 'c': /* int converted to char (check its a char?) */
			      expecttype = ctype_makePointer (ctype_makeConj (ctype_char, ctype_uchar));
			      /*@switchbreak@*/ break;
			      
			    case 's': /* string */
			      expecttype = ctype_string;
			      /*@switchbreak@*/ break;

			    case '[': 
			      /* skip to ']' */
			      while (((key = *(++code)) != ']') 
				     && (key != '\0'))
				{
				  fileloc_incColumn (formatloc);
				}
			      
			      if (key == '\0')
				{
				  llfatalerrorLoc
				    (message ("Bad character set format: %s", 
					      cstring_fromChars (origcode)));
				}
			      
			      expecttype = ctype_string;
			      /*@switchbreak@*/ break;
			      
			    case 'p': /* pointer */
			      expecttype = ctype_unknown;
			      /* really! */
			      /*@switchbreak@*/ break;
			      
			    case 'n': /* pointer to int, modified by call! */
			      expecttype = ctype_makePointer (ctype_int);
			      /*@switchbreak@*/ break;
			  
			    default:
			      expecttype = ctype_unknown;
			      
			      voptgenerror
				(FLG_FORMATCODE,
				 message ("Unrecognized format code: %s", 
					  cstring_fromChars (origcode)),
				 fileloc_isDefined (formatloc) 
				 ? formatloc : g_currentloc);
			      
			      /*@switchbreak@*/ break;
			    }
			  
			  if (!(exprNode_matchArgType (expecttype, a)))
			    {
			      if (ctype_isVoidPointer (expecttype) 
				  && ctype_isRealAbstract (a->typ)
				  && (context_getFlag (FLG_ABSTVOIDP)))
				{
				  ;
				}
			      else
				{
				  if (modifier != '\0')
				    {
				      if (llgenformattypeerror 
					  (expecttype, exprNode_undefined,
					   a->typ, a,
					   message ("Format argument %d to %q (%%%h%h) expects "
						    "%t gets %t: %s",
						    i - argno,
						    uentry_getName (fcn), 
						    modifier,
						    key, expecttype,
						    a->typ, exprNode_unparse (a)),
					   a->loc))
					{
					  if (fileloc_isDefined (formatloc)
					      && context_getFlag (FLG_SHOWCOL))
					    {
					      llgenindentmsg
						(cstring_makeLiteral 
						 ("Corresponding format code"),
						 formatloc);
					    }
					}

				    }
				  else
				    {
				      if (llgenformattypeerror 
					  (expecttype, exprNode_undefined,
					   a->typ, a,
					   message ("Format argument %d to %q (%%%h) expects "
						    "%t gets %t: %s",
						    i - argno,
						    uentry_getName (fcn), 
						    key, expecttype,
						    a->typ, exprNode_unparse (a)),
					   a->loc))
					{
					  if (fileloc_isDefined (formatloc)
					      && context_getFlag (FLG_SHOWCOL))
					    {
					      llgenindentmsg
						(cstring_makeLiteral 
						 ("Corresponding format code"),
						 formatloc);
					    }
					}
				    }
				}
			    }
			  
			  uentry_setType (outArg, expecttype);
			  checkOneArg (outArg, a, f, FALSE, i+1, nargs);
			  uentry_setType (outArg, ctype_unknown);
			  uentry_fixupSref (outArg);
			  
			  if (modified)
			    {
			      exprNode_checkCallModifyVal (a->sref, args, f, ret);
			    }
			}
		      else
			{
			  			  /* a->sref = defref; */
			}
		    }
		}
	    }
	  ocode = code;
	}

      if (i < nargs)
	{
	  voptgenerror (FLG_TYPE,
			message ("Format string for %q has %d arg%p, given %d", 
				 uentry_getName (fcn), i - argno, nargs - argno),
			f->loc);
	}
    }
  else
    {
      /* no checking possible for compile-time unknown format strings */
    }

  fileloc_free (formatloc);
}
			  
static void
checkMessageArgs (/*@notnull@*/ /*@dependent@*/ exprNode f,
		  uentry fcn,
		  exprNodeList args,
		  /*@unused@*/ int argno)
{
  /*
  ** the last argument before the elips is the format string 
  */

  int nargs = exprNodeList_size (args);
  int i = argno;
  fileloc formatloc;
  exprNode a; 

  a = exprNodeList_getN (args, argno - 1);
  formatloc = fileloc_copy (exprNode_loc (a));

  if (ctype_isUnknown (cstringType)) {
    if (usymtab_existsType (cstring_makeLiteralTemp ("cstring")))
      {
	cstringType = usymtab_lookupAbstractType (cstring_makeLiteralTemp ("cstring"));
      }
  }
 
  if (ctype_isUnknown (ctypeType)) {
    if (usymtab_existsType (cstring_makeLiteralTemp ("ctype")))
      {
	ctypeType = usymtab_lookupAbstractType (cstring_makeLiteralTemp ("ctype"));
      }
  }

  if (ctype_isUnknown (filelocType)) {
    if (usymtab_existsType (cstring_makeLiteralTemp ("fileloc")))
      {
	filelocType = usymtab_lookupAbstractType (cstring_makeLiteralTemp ("fileloc"));
      }
  }

  if (exprNode_isDefined (a) && exprNode_isStringLiteral (a) 
      && exprNode_knownStringValue (a))
    {
      cstring format = multiVal_forceString (exprNode_getValue (a));
      char *code = cstring_toCharsSafe (format);
      char *ocode = code;

      nargs = exprNodeList_size (args);

      while ((code = strchr (code, '%')) != NULL)
        {
          char *origcode = code;
          char key = *(++code);			
	  bool isOnly = FALSE;

	  fileloc_addColumn (formatloc, code - ocode);

	  while (key >= '0' && key <= '9')
	    {
	      key = *(++code);
	      fileloc_incColumn (formatloc);
	    }
	  
	  ++code;
	  fileloc_incColumn (formatloc);

          if (key != '%') 
	    {
	      if (key == 'p')
		{
		  goto nextKey;
		}

	      if (i >= nargs)
		{
		  voptgenerror
		    (FLG_TYPE,
		     message ("Message missing format arg %d (%%%h): \"%s\"",
			      i + 1, key, format), 
		     f->loc);
		  i++;
		}
	      else
		{
		  a = exprNodeList_getN (args, i);
		  i++;
		  
		nextKey:
		  if (!exprNode_isError (a))
		    {
		      ctype expecttype;
		      
		      /*@-loopswitchbreak@*/

		      switch (key)
			{
			case 'c':
			case 'h': 
			  expecttype = ctype_char; break;
			case 's': 
			  expecttype = cstringType; break;
			case 'q': 
			  expecttype = cstringType; isOnly = TRUE; break;
			case 'x': 
			  expecttype = cstringType; isOnly = TRUE; break;
			case 'd': expecttype = ctype_int; break;
			case 'u': expecttype = ctype_uint; break;
			case 'w': expecttype = ctype_ulint; break;
			case 'f': expecttype = ctype_float; break;
			case 'b': expecttype = ctype_bool; break;
			case 't': expecttype = ctypeType; break;
			case 'l': expecttype = filelocType; break;
			case 'p':  /* a wee bit of a hack methinks */
			  expecttype = ctype_int;
			  break;
			case 'r': expecttype = ctype_bool; break;
			default:  
			  expecttype = ctype_unknown;
			  voptgenerror
			    (FLG_FORMATCODE,
			     message ("Unrecognized format code: %s", 
				      cstring_fromChars (origcode)),
			     fileloc_isDefined (formatloc) 
			     ? formatloc : g_currentloc);
			  break;
			}
		      /*@=loopswitchbreak@*/

		      if (!(exprNode_matchArgType (expecttype, a)))
			{
			  if (ctype_isVoidPointer (expecttype) 
			      && ctype_isRealAbstract (a->typ)
			      && (context_getFlag (FLG_ABSTVOIDP)))
			    {
			      ;
			    }
			  else
			    {
			      if (llgenformattypeerror 
				  (expecttype, exprNode_undefined,
				   a->typ, a,
				   message ("Format argument %d to %q (%%%h) expects "
					    "%t gets %t: %s",
					    i - argno,
					    uentry_getName (fcn), 
					    key, expecttype,
					    a->typ, exprNode_unparse (a)),
				   a->loc))
				  {
				  if (fileloc_isDefined (formatloc)
				      && context_getFlag (FLG_SHOWCOL))
				    {
				      llgenindentmsg
					(cstring_makeLiteral 
					 ("Corresponding format code"),
					 formatloc);
				    }
				}
			    }
			}
		      
		      if (ctype_equal (expecttype, cstringType))
			{
			  if (isOnly)
			    {
			      checkOneArg (csOnlyArg, a, f, FALSE, i+1, nargs);
			      uentry_fixupSref (csOnlyArg);
			    }
			  else
			    {
			      checkOneArg (csArg, a, f, FALSE, i+1, nargs);
			      uentry_fixupSref (csArg);
			    }
			}
		      else
			{
			  			  checkOneArg (regArg, a, f, FALSE, i+1, nargs);
			  uentry_fixupSref (regArg);
			}
		    }
		}
	    }
	}

      if (i < nargs)
	{
	  voptgenerror (FLG_TYPE,
			message ("Format string for %q has %d arg%p, given %d", 
				 uentry_getName (fcn), i - argno, nargs -argno),
			f->loc);
	}
    }
  else
    {
      /* no checking possible for compile-time unknown format strings */
    }

  fileloc_free (formatloc);
}

static void
  checkExpressionDefinedAux (/*@notnull@*/ exprNode e1, 
			     /*@notnull@*/ exprNode e2, 
			     sRefSet sets1,
			     sRefSet sets2, 
			     lltok op,
			     flagcode flag)
{
  bool hadUncon = FALSE;

  if (sRef_isGlobal (sRef_getRootBase (e1->sref)) && 
      sRefSet_hasUnconstrained (sets2))
    {
      voptgenerror
	(FLG_EVALORDERUNCON,
	 message
	 ("Expression may have undefined behavior (%q used in right operand "
	  "may set global variable %q used in left operand): %s %s %s", 
	  sRefSet_unparseUnconstrained (sets2),
	  sRef_unparse (sRef_getRootBase (e1->sref)),
	  exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	 e2->loc);
    }

  if (sRef_isGlobal (sRef_getRootBase (e2->sref)) && 
      sRefSet_hasUnconstrained (sets1))
    {
      voptgenerror
	(FLG_EVALORDERUNCON,
	 message
	 ("Expression has undefined behavior (%q used in left operand "
	  "may set global variable %q used in right operand): %s %s %s", 
	  sRefSet_unparseUnconstrained (sets1),
	  sRef_unparse (e2->sref),
	  exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	 e2->loc);
    }

  sRefSet_realElements (e1->uses, sr)
    {
      if (sRef_isMeaningful (sr) && sRefSet_member (sets2, sr))
	{
	  voptgenerror
	    (FLG_EVALORDER,
	     message
	     ("Expression has undefined behavior (left operand uses %q, "
	      "modified by right operand): %s %s %s", 
	      sRef_unparse (sr),
	      exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	     e2->loc);
	}
    } end_sRefSet_realElements;
  
  sRefSet_realElements (sets1, sr)
    {
      if (sRef_isMeaningful (sr))
	{
	  if (sRef_same (sr, e2->sref))
	    {
	      voptgenerror
		(flag,
		 message
		 ("Expression has undefined behavior (value of right operand "
		  "modified by left operand): %s %s %s", 
		  exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
		 e2->loc);
	    }
	  else if (sRefSet_member (e2->uses, sr))
	    {
	      voptgenerror
		(flag,
		 message 
		 ("Expression has undefined behavior (left operand modifies %q, "
		  "used by right operand): %s %s %s", 
		  sRef_unparse (sr),
		  exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
		 e2->loc);
	    }
	  else 
	    {
	      if (sRefSet_member (sets2, sr))
		{
		  if (sRef_isUnconstrained (sr))
		    {
		      if (hadUncon)
			{
			  ;
			}
		      else
			{
			  hadUncon = optgenerror
			    (FLG_EVALORDERUNCON,
			     message 
			     ("Expression may have undefined behavior.  Left operand "
			      "calls %q; right operand calls %q.  The unconstrained "
			      "functions may modify global state used by "
			      "the other operand): %s %s %s", 
			      sRefSet_unparseUnconstrained (sets1),
			      sRefSet_unparseUnconstrained (sets2),
			      exprNode_unparse (e1), lltok_unparse (op),
			      exprNode_unparse (e2)),
			     e2->loc);
			}
		    }
		  else
		    {
		      voptgenerror
			(flag,
			 message 
			 ("Expression has undefined behavior (both "
			  "operands modify %q): %s %s %s", 
			  sRef_unparse (sr), 
			  exprNode_unparse (e1), 
			  lltok_unparse (op), exprNode_unparse (e2)),
			 e2->loc);
		    }
		}
	    }
	}
    } end_sRefSet_realElements;
}

static void checkExpressionDefined (exprNode e1, exprNode e2, lltok op)
{
  bool hasError = FALSE;
  
  if (exprNode_isError (e1) || exprNode_isError (e2))
    {
      return;
    }

  if (sRefSet_member (e2->sets, e1->sref))
    {
      if (e2->kind == XPR_CALL)
	{
	 ;
	}
      else
	{
	  hasError = optgenerror 
	    (FLG_EVALORDER,
	     message ("Expression has undefined behavior "
		      "(value of left operand is modified "
		      "by right operand): %s %s %s", 
		      exprNode_unparse (e1), lltok_unparse (op),
		      exprNode_unparse (e2)),
	     e2->loc);
	}
    }

  if (context_getFlag (FLG_EVALORDERUNCON))
    {
      if (sRefSet_member (e2->msets, e1->sref))
	{
	  if (e2->kind == XPR_CALL)
	    {
	      ;
	    }
	  else
	    {
	      hasError = optgenerror 
		(FLG_EVALORDER,
		 message 
		 ("Expression has undefined behavior (value of left "
		  "operand may be modified by right operand): %s %s %s", 
		  exprNode_unparse (e1), lltok_unparse (op),
		  exprNode_unparse (e2)),
		 e2->loc);
	    }
	}
    }
  
  if (!hasError)
    {
      checkExpressionDefinedAux (e1, e2, e1->sets, e2->sets, op, FLG_EVALORDER);
      
      if (context_maybeSet (FLG_EVALORDERUNCON))
	{
	  checkExpressionDefinedAux (e1, e2, e1->msets, 
				     e2->msets, op, FLG_EVALORDERUNCON);
	}
    }
}

static void checkSequencing (exprNode p_f, exprNodeList p_args); 

static int
  checkArgsReal (uentry fcn, /*@dependent@*/ exprNode f, uentryList cl, 
		 exprNodeList args, bool isIter, exprNode ret)
{
  int special = 0;

  if (!exprNode_isError (f))
    {
      if (!uentryList_isMissingParams (cl))
        {
          int nargs = exprNodeList_size (args);
          int expectargs = uentryList_size (cl);
          ctype last;
          int i = 0;
	  
          if (expectargs == 0)
            {
              if (nargs != 0)
		{
		  if (isIter)
		    {
		      voptgenerror
			(FLG_TYPE,
			 message ("Iter %q invoked with %d args, "
				  "declared void",
				  uentry_getName (fcn),
				  nargs),
			 f->loc);
		    }
		  else
		    {
		      voptgenerror 
			(FLG_TYPE,
			 message ("Function %s called with %d args, "
				  "declared void", 
				  exprNode_unparse (f), nargs),
			 f->loc);
		    }
		}
              return special;
            }
	  
          last = uentry_getType (uentryList_getN (cl, expectargs - 1));
	  
          exprNodeList_reset (args);
	  
          uentryList_elements (cl, current)
            {
              ctype ct = uentry_getType (current);
              exprNode a;
	      
              if (ctype_isElips (ct))
		{
		  /*
		   ** do special checking for printf/scanf library functions
		   **
		   ** this is kludgey code, just for handling the special case
		   **
		   */

		  if (uentry_isPrintfLike (fcn))
		    {
		      checkPrintfArgs (f, fcn, args, ret, i);
		      special = i;
		    }
		  else if (uentry_isScanfLike (fcn))
		    {
		      checkScanfArgs (f, fcn, args, ret, i); 
		      special = i;
		    }
		  else if (uentry_isMessageLike (fcn))
		    {
		      checkMessageArgs (f, fcn, args, i);
		      special = i;
		    }
		  else
		    {
		      llassert (!uentry_isSpecialFunction (fcn));
		    }
		    
		  nargs = expectargs; /* avoid errors */
		  break;
		}
              else
		{
		  if (i >= nargs) break;
		  
		  a = exprNodeList_current (args);
		  exprNodeList_advance (args);
		  
		  i++;
		  
		  if (exprNode_isError (a))
		    {
		     ;
		    }
		  else
		    {
		      /* 
			probably necessary? I'm not sure about this one
			checkMacroParen (a);
			*/
		      
		      f->guards = guardSet_union (f->guards, a->guards);

		      if (!(exprNode_matchArgType (ct, a)))
			{
			  if (ctype_isVoidPointer (ct) 
			      && (ctype_isPointer (a->typ) 
				  && (ctype_isRealAbstract (ctype_baseArrayPtr (a->typ)))))
			    {
			      vnoptgenerror 
				(FLG_ABSTVOIDP,
				 message 
				 ("Pointer to abstract type (%t) used "
				  "as void pointer "
				  "(arg %d to %q): %s",
				  a->typ, i, 
				  uentry_getName (fcn), 
				  exprNode_unparse (a)),
				 a->loc);
			    }
			  else
			    {
			      if (isIter)
				{
				  (void) gentypeerror 
				    (ct, exprNode_undefined,
				     a->typ, a,
				     message 
				     ("Iter %q expects arg %d to "
				      "be %t gets %t: %s",
				      uentry_getName (fcn),
				      i, ct, a->typ, exprNode_unparse (a)),
				     a->loc);
				}
			      else
				{
				  if (gentypeerror  
				      (ct, 
				       exprNode_undefined,
				       a->typ,
				       a,
				       message 
				       ("Function %q expects arg %d to be %t gets %t: %s",
					uentry_getName (fcn),
					i, ct, a->typ, exprNode_unparse (a)),
				       a->loc))
				    {
				      DPRINTF (("Types: %s / %s",
						ctype_unparse (ct),
						ctype_unparse (a->typ)));
				    }

				  /*
				  ** Clear null marker for abstract types.
				  ** (It is not revealed, so suppress future messages.)
				  */

				  if (ctype_isAbstract (a->typ))
				    {
				      sRef_setNullUnknown (exprNode_getSref (a), a->loc);
				    }
				}
			    }
			}
		    }
		}
            } end_uentryList_elements ;
	  
	  
	  if (expectargs != nargs) /* note: not != since we may have ... */
	    {
	      if (ctype_isElips (last))
		{
		  voptgenerror
		    (FLG_TYPE,
		     message ("Function %s called with %d args, expects at least %d",
			      exprNode_unparse (f),
			      nargs, expectargs - 1),
		     f->loc);
		}
	      else
		{
		  if (isIter)
		    {
		      voptgenerror 
			(FLG_TYPE,
			 message ("Iter %q invoked with %d args, expects %d",
				  uentry_getName (fcn), nargs, expectargs),
			 f->loc);
		    }
		  else
		    {
		      voptgenerror 
			(FLG_TYPE,
			 message ("Function %s called with %d args, expects %d",
				  exprNode_unparse (f),
				  nargs, expectargs),
			 f->loc);
		    }
		}
	    }
        }
    }

  return special;
}

/*
** Check for undefined code sequences in function arguments:
**
**   one parameter sets something used by another parameter
**   one parameter sets something set  by another parameter
*/

static void 
checkSequencingOne (exprNode f, exprNodeList args, 
		    /*@notnull@*/ exprNode el, int argno)
{
  /*
  ** Do second loop, iff +undefunspec
  */

  int checkloop;
  int numloops = context_maybeSet (FLG_EVALORDERUNCON) ? 2 : 1;
  
  for (checkloop = 0; checkloop < numloops; checkloop++)
    {
      sRefSet thissets;

      if (checkloop == 0)
	{
	  thissets = el->sets;
	}
      else
	{
	  llassert (checkloop == 1);
	  thissets = el->msets;
	}
      
      sRefSet_realElements (thissets, thisset)
	{
	  int j;

	  /*@access exprNodeList@*/
	  for (j = 0; j < args->nelements; j++)
	    {
	      exprNode jl = args->elements[j];
	      int thisargno = j + 1;

	      if (thisargno != argno && exprNode_isDefined (jl))
		{
		  sRefSet otheruses = jl->uses;
		  
		  if (sRef_isGlobal (sRef_getRootBase (jl->sref)) && 
		      sRefSet_hasUnconstrained (thissets))
		    {
		      voptgenerror
			(FLG_EVALORDERUNCON,
			 /*@-sefparams@*/
			 message
			 ("%q used in argument %d may set "
			  "global variable %q used by argument %d: %s(%q)", 
			  cstring_capitalizeFree (sRefSet_unparseUnconstrained (thissets)),
			  /*@=sefparams@*/
			  argno,
			  sRef_unparse (sRef_getRootBase (jl->sref)),
			  thisargno,
			  exprNode_unparse (f), exprNodeList_unparse (args)),
			 el->loc);
		    }

		  if (sRefSet_member (otheruses, thisset))
		    {
		      if (sRef_isUnconstrained (thisset))
			{
			  voptgenerror 
			    (FLG_EVALORDERUNCON,
			     message 
			     ("Unconstrained functions used in arguments %d (%q) "
			      "and %d (%s) may modify "
			      "or use global state in undefined way: %s(%q)",
			      argno,
			      sRefSet_unparseUnconstrainedPlain (otheruses),
			      thisargno, 
			      sRef_unconstrainedName (thisset),
			      exprNode_unparse (f), 
			      exprNodeList_unparse (args)),
			     el->loc);
			}
		      else
			{
			  voptgenerror 
			    (FLG_EVALORDER,
			     message 
			     ("Argument %d modifies %q, used by argument %d "
			      "(order of evaluation of actual parameters is "
			      "undefined): %s(%q)",
			      argno, sRef_unparse (thisset), thisargno, 
			      exprNode_unparse (f), exprNodeList_unparse (args)),
			     el->loc);
			}
		    }
		  else 
		    {
		      sRefSet othersets = jl->sets;
		      
		      if (sRefSet_member (othersets, thisset))
			{
			  if (sRef_isUnconstrained (thisset))
			    {
			      voptgenerror 
				(FLG_EVALORDERUNCON,
				 message 
				 ("Unconstrained functions used in "
				  "arguments %d (%q) and %d (%s) may modify "
				  "or use global state in undefined way: %s(%q)",
				  argno, 
				  sRefSet_unparseUnconstrainedPlain (othersets),
				  thisargno, 
				  sRef_unconstrainedName (thisset),
				  exprNode_unparse (f), exprNodeList_unparse (args)),
				 el->loc);
			    }
			  else
			    {
			      voptgenerror 
				(FLG_EVALORDER,
				 message 
				 ("Argument %d modifies %q, set by argument %d (order of"
				  " evaluation of actual parameters is undefined): %s(%q)",
				  argno, sRef_unparse (thisset), thisargno, 
				  exprNode_unparse (f), exprNodeList_unparse (args)),
				 el->loc);
			    }
			}
		    }
		}
	    }
	  /*@noaccess exprNodeList@*/
	} end_sRefSet_realElements;
    }
}

static void
checkSequencing (exprNode f, exprNodeList args)
{
  if (exprNodeList_size (args) > 1)
    {
      int i;
      exprNode el;
      
      /*@access exprNodeList*/
      
      for (i = 0; i < args->nelements; i++)
	{
	  el = args->elements[i];
	  
	  if (!exprNode_isError (el))
	    {
	      checkSequencingOne (f, args, el, i + 1);
	    }
	}
      /*@noaccess exprNodeList*/
    }
}

/*
** requires le = exprNode_getUentry (f) 
*/

static void
checkGlobMods (/*@notnull@*/ /*@dependent@*/ exprNode f,
	       uentry le, exprNodeList args, 
	       /*@notnull@*/ exprNode ret, int specialArgs)
{
  bool isSpec = FALSE;
  bool hasMods = FALSE;
  cstring fname;
  globSet usesGlobs = globSet_undefined;
  sRefSet mods = sRefSet_undefined;
  bool freshMods = FALSE;
  uentryList params = uentryList_undefined;

  /*
  ** check globals and modifies
  */

  setCodePoint ();
  
  if (!uentry_isValid (le))
    {
      ctype fr = ctype_realType (f->typ);

      if (ctype_isFunction (fr))
        {
          params = ctype_argsFunction (fr);
        }
      else
        {
          params = uentryList_missingParams;
        }

      if (!context_getFlag (FLG_MODNOMODS) 
	  && !context_getFlag (FLG_GLOBUNSPEC))
        {
	            checkUnspecCall (f, params, args);
        }
      
      return;
    }

  fname = uentry_rawName (le);

  setCodePoint ();

  if (uentry_isFunction (le))
    {
      params = uentry_getParams (le);
      mods = uentry_getMods (le);
      hasMods = uentry_hasMods (le);
      usesGlobs = uentry_getGlobs (le);
      isSpec = uentry_isSpecified (le);
    }
  else /* not a function */
    {
      ctype ct = ctype_realType (uentry_getType (le));
      
      llassertprint (uentry_isVar (le) && ctype_isFunction (ct),
		     ("checkModGlobs: uentry not a function: %s", 
		      uentry_unparse (le)));
      
      params = ctype_argsFunction (ct);
    }

  /*
  ** check globals
  */

  setCodePoint ();

    
  globSet_allElements (usesGlobs, el)
    {
      if (sRef_isValid (el))
	{
	  if (sRef_isInternalState (el) || sRef_isSystemState (el))
	    {
	      context_usedGlobal (el);
	      exprNode_checkUse (f, el, f->loc);

	      if (context_checkInternalUse ())
		{
		  if (!context_globAccess (el))
		    {
		      if (sRef_isSystemState (el)
			  && !context_getFlag (FLG_MODFILESYSTEM))
			{
			  ;
			}
		      else
			{
			  voptgenerror
			    (FLG_INTERNALGLOBS,
			     message 
			     ("Called procedure %s may access %q, but "
			      "globals list does not include globals %s",
			      exprNode_unparse (f),
			      sRef_unparse (el),
			      cstring_makeLiteralTemp (sRef_isInternalState (el)
						       ? "internalState"
						       : "fileSystem")),
			     f->loc);
			}
		    }
		}
	    }
	  else if (sRef_isNothing (el) || sRef_isSpecState (el))
	    {
	      ;
	    }
	  else
	    {
	      uentry gle = sRef_getUentry (el);
	      sRef sr = sRef_updateSref (el);
	      
	      if (sRef_isUndefGlob (el))
		{
		  sRef_setDefined (sr, f->loc);
		  exprNode_checkSet (f, sr);
		}
	      else
		{
		  /*
		  ** check definition
		  */
		  
		  if (sRef_isAllocated (el))
		    {
		      exprNode_checkSet (f, sr);
		    }
		  else
		    {
		      if (sRef_isStateUndefined (sr))
			{
			  voptgenerror 
			    (FLG_GLOBSTATE,
			     message
			     ("%s %q used by function undefined before call: %s",
			      sRef_getScopeName (sr),
			      sRef_unparse (sr),
			      exprNode_unparse (f)),
			     f->loc);
			  sRef_setDefined (sr, f->loc);
			}
		      exprNode_checkUse (f, sr, f->loc);
		    }
		  
		  checkGlobUse (gle, TRUE, f);
		}
	  
	      if (sRef_isKilledGlob (el))
		{
		  sRef_kill (sr, f->loc);
		  context_usedGlobal (sr);
		}
	    }
	}
    } end_globSet_allElements;
  
  /*
  ** check modifies
  */

  if (context_hasMods () || context_getFlag (FLG_MODNOMODS))
    {
      sRefSet smods = sRefSet_undefined;

      /*
      ** NEED to check for modifies anything
      */

      /*
      ** check each sRef that called function modifies (ml), is
      ** modifiable by tl
      */

      setCodePoint ();

      sRefSet_allElements (mods, s) /* s is something which may be modified */
        {
	  if (sRef_isKindSpecial (s))
	    {
	      if (sRef_isSpecInternalState (s))
		{
		  if (context_getFlag (FLG_MODINTERNALSTRICT))
		    {
		      exprNode_checkCallModifyVal (s, args, f, ret);
		    }
		  else
		    {
		      sRefSet mmods = context_modList ();

		      sRefSet_allElements (mmods, el)
			{
			  if (sRef_isInternalState (el))
			    {
			      sRef_setModified (el);
			    }
			} end_sRefSet_allElements ;
		    }
		}
	      else
		{
		  exprNode_checkCallModifyVal (s, args, f, ret);
		}
	    }
	  else
	    {
	      sRef rb = sRef_getRootBase (s);
	      
	      if (sRef_isGlobal (rb))
		{
		  context_usedGlobal (rb);
		}
	      
	      if (sRef_isFileStatic (s)
		  && !fileId_equal (fileloc_fileId (f->loc), 
				    fileloc_fileId (uentry_whereDefined (le))))
		{
		  smods = sRefSet_insert (smods, s);
		}
	      else
		{
		  exprNode_checkCallModifyVal (s, args, f, ret);
		}
	    }
        } end_sRefSet_allElements;

      setCodePoint ();

      /*
      ** Static elements in modifies set can have nasty consequences.
      ** (I think...have not been able to reproduce a possible bug.)
      */

      if (!sRefSet_isDefined (smods))
	{
	  mods = sRefSet_newCopy (mods);
	  freshMods = TRUE;
	  
	  sRefSet_allElements (smods, el)
	    {
	      bool res = sRefSet_delete (mods, el);
	      
	      llassert (res);
	    } end_sRefSet_allElements;

	  sRefSet_free (smods);
	/*@-branchstate@*/
	} 
      /*@=branchstate@*/
    }
  else if (sRefSet_isDefined (mods))
    { /* just check observers */
      setCodePoint ();

      sRefSet_allElements (mods, s) /* s is something which may be modified */
        {
	  sRef rb = sRef_getRootBase (s);

	  setCodePoint ();

	  if (sRef_isParam (rb))
	    {
	      sRef b = sRef_fixBaseParam (s, args);

	      if (sRef_isObserver (b))
		{
		  exprNode e = exprNodeList_nth (args, sRef_getParam (rb));
		  
		  if (optgenerror 
		      (FLG_MODOBSERVER,
		       message ("Function call may modify observer%q: %s", 
				sRef_unparsePreOpt (b), exprNode_unparse (e)),
		       exprNode_loc (e)))
		    {
		      sRef_showExpInfo (b);
		    }
		}
	    }
        } end_sRefSet_allElements;
    }
  else 
    {
      if (!hasMods) /* no specified modifications */
	{
	  if (context_getFlag (FLG_MODOBSERVERUNCON))
	    {
	      exprNodeList_elements (args, e)
		{
		  if (exprNode_isDefined (e))
		    {
		      sRef s = exprNode_getSref (e);
		      
		      if (sRef_isObserver (s) 
			  && ctype_isMutable (sRef_getType (s)))
			{
			  if (optgenerror 
			      (FLG_MODOBSERVERUNCON,
			       message
			       ("Call to unconstrained function %s may modify observer%q: %s", 
				exprNode_unparse (f),
				sRef_unparsePreOpt (s), exprNode_unparse (e)),
			       exprNode_loc (e)))
			    {
			      sRef_showExpInfo (s);
			    }
			}
		    }
		} end_exprNodeList_elements; 
	    }
	}
    }
  
  checkAnyCall (f, fname, params, args, hasMods, mods, isSpec, specialArgs);

  ret->uses = sRefSet_union (ret->uses, f->uses);
  ret->sets = sRefSet_union (ret->sets, f->sets);
  ret->msets = sRefSet_union (ret->msets, f->msets);

  if (freshMods)
    {
      /*
      ** Spurious errors reported, because lclint can't tell
      ** mods must be fresh if freshMods is true.
      */

      /*@i@*/ sRefSet_free (mods);
    }

  setCodePoint ();
}

void checkGlobUse (uentry glob, bool isCall, /*@notnull@*/ exprNode e)
{
  if (uentry_isVar (glob))
    {
      if (context_inFunctionLike ())
	{
	  sRef sr = uentry_getSref (glob);

	  context_usedGlobal (sr);
	  
	  if (context_checkGlobUse (glob))
	    {
	      if (!context_globAccess (sr))
		{
		  if (isCall)
		    {
		      voptgenerror
			(FLG_GLOBALS,
			 message ("Called procedure %s may access %s %q",
				  exprNode_unparse (e), 
				  sRef_unparseScope (sr),
				  uentry_getName (glob)),
			 e->loc);
		    }
		  else
		    {
		      voptgenerror 
			(FLG_GLOBALS,
			 message ("Undocumented use of %s %s", 
				  sRef_unparseScope (sr),
				  exprNode_unparse (e)),
			 e->loc);
		    }
		}
	    }
	}
    }
  else
    {
      llbug (message ("Global not variable: %q", uentry_unparse (glob)));
    }
}  

static /*@only@*/ exprNode
functionCallSafe (/*@only@*/ /*@notnull@*/ exprNode f,
		  ctype t, /*@keep@*/ exprNodeList args)
{
  /* requires f is a non-error exprNode, with type function */
  cstring fname = exprNode_unparse (f);
  uentry le = exprNode_getUentry (f);
  exprNode ret = exprNode_createPartialCopy (f);
  int special;

  setCodePoint ();
        
  ret->typ = ctype_returnValue (t);
  ret->kind = XPR_CALL;

  ret->edata = exprData_makeCall (f, args);

  /*
  ** Order of these steps is very important!  
  **
  ** Must check for argument dependencies before messing up uses and sets.
  */

  if (context_getFlag (FLG_EVALORDER))
    {
      exprNodeList_elements (args, current)
	{
	  if (exprNode_isDefined (current))
	    {
	      exprNode_addUse (current, current->sref);
	    }
	} end_exprNodeList_elements;

      if (context_maybeSet (FLG_EVALORDER) || context_maybeSet (FLG_EVALORDERUNCON))
	{
	  checkSequencing (f, args); 
	}
      
      exprNodeList_elements (args, current)
	{
	  if (exprNode_isDefined (current) && sRef_isMeaningful (current->sref))
	    {
	      exprNode_addUse (ret, sRef_makeDerived (current->sref));
	    }
	} end_exprNodeList_elements ;
    }

  special = checkArgs (le, f, t, args, ret); 
  checkGlobMods (f, le, args, ret, special); 

  setCodePoint ();

  if (uentry_isValid (le)
      && (uentry_isFunction (le) 
          || (uentry_isVariable (le)
	      && ctype_isFunction (uentry_getType (le)))))
    {
      exitkind exk = uentry_getExitCode (le);

      /* f->typ is already set to the return type */

      ret->sref = uentry_returnedRef (le, args);

      if (uentry_isFunction (le) && exprNodeList_size (args) >= 1)
	{
	  qual nullPred = uentry_nullPred (le);

	  if (qual_isTrueNull (nullPred))
	    {
	      exprNode arg = exprNodeList_head (args);

	      if (exprNode_isDefined (arg))
		{
		  ret->guards = guardSet_addFalseGuard (ret->guards, arg->sref);
		}
	    }
	  else if (qual_isFalseNull (nullPred))
	    {
	      exprNode arg = exprNodeList_head (args);
	      
	      if (exprNode_isDefined (arg))
		{
		  ret->guards = guardSet_addTrueGuard (ret->guards, arg->sref);
		}
	    }
	  else
	    {
	      llassert (qual_isUnknown (nullPred));
	    }
	}
      
      if (exitkind_isConditionalExit (exk))
	{
	  /*
	  ** True exit is: 
	  **    if (arg0) then { exit! } else { ; }
	  ** False exit is:
	  **    if (arg0) then { ; } else { exit! }
	  */

	  exprNode firstArg;

	  llassert (!exprNodeList_isEmpty (args));
	  firstArg = exprNodeList_head (args);

	  if (exprNode_isDefined (firstArg)
	      && !guardSet_isEmpty (firstArg->guards))
	    {
	      usymtab_trueBranch (guardSet_undefined);
	      usymtab_altBranch (guardSet_undefined);
	      
	      if (exitkind_isTrueExit (exk))
		{
		  usymtab_popBranches (firstArg, 
				       exprNode_makeMustExit (), 
				       exprNode_undefined,
				       TRUE, TRUEEXITCLAUSE);
		}
	      else
		{
		  usymtab_popBranches (firstArg,
				       exprNode_undefined,
				       exprNode_makeMustExit (), 
				       TRUE, FALSEEXITCLAUSE);
		}
	    }

	  ret->exitCode = XK_MAYEXIT;
	}
      else if (exitkind_mustExit (exk))
	{
	  ret->exitCode = XK_MUSTEXIT;
	}
      else if (exitkind_couldExit (exk))
	{
	  ret->exitCode = XK_MAYEXIT;
	}
      else
	{
	  ;
	}
      
      if (cstring_equalLit (fname, "exit"))
	{
	  if (exprNodeList_size (args) == 1)
	    {
	      exprNode arg = exprNodeList_head (args);
	      
	      if (exprNode_isDefined (arg) && exprNode_knownIntValue (arg))
		{
		  long int val = multiVal_forceInt (exprNode_getValue (arg));
		  
		  if (val != 0)
		    {
		      voptgenerror
			(FLG_EXITARG,
			 message 
			 ("Argument to exit has implementation defined behavior: %s",
			  exprNode_unparse (arg)),
			 exprNode_loc (arg));
		    }
		}
	    }
	}
    }
  else
    {
      ret->sref = defref;
      exprNode_checkSetAny (ret, uentry_rawName (le));
    }

  return (ret);
}

/*
** this is yucky!  should keep the uentry as part of exprNode!
*/

/*@observer@*/ uentry
exprNode_getUentry (exprNode e)
{
  if (exprNode_isError (e))
    {
      return uentry_undefined;
    }
  else
    {
      cstring s = exprNode_rootVarName (e);
      uentry ue = usymtab_lookupSafe (s);

      return ue;
    }
}

exprNode 
exprNode_makeInitBlock (lltok brace, /*@only@*/ exprNodeList inits)
{
  exprNode ret = exprNode_createPlain (ctype_unknown);

  ret->kind = XPR_INITBLOCK;
  ret->edata = exprData_makeCall (exprNode_undefined, inits);
  ret->loc = fileloc_update (ret->loc, lltok_getLoc (brace));

  return (ret);
}

exprNode
exprNode_functionCall (/*@only@*/ exprNode f, /*@only@*/ exprNodeList args)
{
  ctype t;

  setCodePoint ();

  if (exprNode_isUndefined (f))
    {
      exprNode_free (f);
      exprNodeList_free (args);
      return exprNode_undefined;
    }

  t = exprNode_getType (f);

  if (sRef_isLocalVar (f->sref))
    {
      exprNode_checkUse (f, f->sref, f->loc);

      if (sRef_possiblyNull (f->sref))
        {
          if (!usymtab_isGuarded (f->sref))
            {
              if (optgenerror (FLG_NULLDEREF,
			       message ("Function call using %s pointer %q", 
					sRef_nullMessage (f->sref),
					sRef_unparse (f->sref)),
			       f->loc))
		{
		  sRef_showNullInfo (f->sref);
		  sRef_setNullError (f->sref);
		}
            }
        }
    }

  setCodePoint ();

  if (ctype_isRealFunction (t))
    {
      exprNode ret = functionCallSafe (f, t, args);
      setCodePoint ();
      return ret;
    }
  else if (ctype_isUnknown (t))
    {
      exprNode ret = exprNode_createPartialCopy (f);
      cstring tstring;

      setCodePoint ();
      
      ret->typ = t;
      exprNodeList_elements (args, current)
        {
          if (exprNode_isDefined (current))
            {
              exprNode_checkUse (ret, current->sref, ret->loc);

	      /* 
	      ** also, anything derivable from current->sref may be used 
	      */

	      exprNode_addUse (ret, sRef_makeDerived (current->sref));
              exprNode_mergeUSs (ret, current);
            }
        } end_exprNodeList_elements;

      ret->edata = exprData_makeCall (f, args);
      ret->kind = XPR_CALL;

      tstring = cstring_copy (exprNode_unparse (f));

      cstring_markOwned (tstring);
      exprNode_checkSetAny (ret, tstring);

      return (ret);
    }
  else
    {
      voptgenerror (FLG_TYPE,
		    message ("Call to non-function (type %t): %s", t, 
			     exprNode_unparse (f)),
		    f->loc);
      exprNode_free (f);
      exprNodeList_free (args);

      return (exprNode_makeError ());
    }
}

exprNode
exprNode_fieldAccess (/*@only@*/ exprNode s, /*@only@*/ cstring f)
{
  exprNode ret = exprNode_createPartialCopy (s);

  ret->kind = XPR_FACCESS;

  if (exprNode_isError (s))
    {
      ret->edata = exprData_makeField (s, f);
      return ret;
    }
  else
    {
      ctype t = exprNode_getType (s);
      ctype tr = ctype_realType (t);
      
      checkMacroParen (s);

      ret->edata = exprData_makeField (s, f);
      
      if (ctype_isStructorUnion (tr))
        {
          uentry tf = uentryList_lookupField (ctype_getFields (tr), f);

          if (uentry_isUndefined (tf))
            {
              voptgenerror (FLG_TYPE,
			    message ("Access non-existent field %s of %t: %s", f, t, 
				     exprNode_unparse (ret)),
			    s->loc);
	      
              return (ret);
            }
          else
            {
	      uentry_setUsed (tf, exprNode_loc (ret));

              ret->typ = uentry_getType (tf); 
              checkSafeUse (ret, s->sref);
	      
              ret->sref = sRef_makeField (s->sref, uentry_rawName (tf));
              return (ret);
            }
        }
      else /* isStructorUnion */
        {
          if (ctype_isRealAbstract (tr))
            {
              voptgenerror
		(FLG_ABSTRACT,
		 message ("Access field of abstract type (%t): %s.%s", 
			  t, exprNode_unparse (s), f),
		 s->loc);
              ret->typ = ctype_unknown;
            }
          else
            {
              if (ctype_isKnown (tr))
		{
		  voptgenerror 
		    (FLG_TYPE,
		     message
		     ("Access field of non-struct or union (%t): %s.%s",
		      t, exprNode_unparse (s), f),
		     s->loc);

		  ret->typ = ctype_unknown;
		}
              else
		{
		  cstring sn = cstring_copy (f);

		  checkSafeUse (ret, s->sref);
		  cstring_markOwned (sn);
		  ret->sref = sRef_makeField (s->sref, sn);

		  return (ret);
		}
            }
          return (ret);
        }
    }
  BADEXIT;
}

exprNode
exprNode_addParens (/*@only@*/ lltok lpar, /*@only@*/ exprNode e)
{
  exprNode ret = exprNode_createPartialCopy (e);

  ret->loc = fileloc_update (ret->loc, lltok_getLoc (lpar));
  ret->kind = XPR_PARENS;
  ret->edata = exprData_makeUop (e, lpar);

  if (!exprNode_isError (e))
    {
      ret->exitCode = e->exitCode;
      ret->canBreak = e->canBreak;
      ret->mustBreak = e->mustBreak;
      ret->isJumpPoint = e->isJumpPoint;
      ret->sref = e->sref;
    }

  return ret;
}

exprNode
exprNode_arrowAccess (/*@only@*/ exprNode s, /*@only@*/ cstring f)
{
  exprNode ret = exprNode_createPartialCopy (s);

  ret->edata = exprData_makeField (s, f);
  ret->kind = XPR_ARROW;

  if (exprNode_isError (s))
    {
      return (ret);
    }
  else
    {
       ctype t = exprNode_getType (s);
       ctype tr = ctype_realType (t);

       checkMacroParen (s);

       (void) ctype_fixArrayPtr (tr); /* REWRITE THIS */

       if (ctype_isRealPointer (tr)) 
	 {
	   ctype b = ctype_realType (ctype_baseArrayPtr (tr));
	   
	   if (ctype_isStructorUnion (b))
	     {
	       uentry fentry = uentryList_lookupField (ctype_getFields (b), f);
	       
	       if (sRef_isKnown (s->sref) && sRef_possiblyNull (s->sref))
		 {
		   if (!usymtab_isGuarded (s->sref) && !context_inProtectVars ())
		     {
		       if (optgenerror 
			   (FLG_NULLDEREF,
			    message ("Arrow access from %s pointer%q: %s", 
				     sRef_nullMessage (s->sref),
				     sRef_unparsePreOpt (s->sref),
				     exprNode_unparse (ret)),
			    s->loc))
			 {
			   sRef_showNullInfo (s->sref);
			   sRef_setNullError (s->sref);
			 }
		     }
		 }
	       
	       if (uentry_isUndefined (fentry))
		 {
		   voptgenerror 
		     (FLG_TYPE,
		      message ("Access non-existent field %s of %t: %s", 
			       f, t, exprNode_unparse (ret)),
		      s->loc);
		   ret->typ = ctype_unknown;
		   
		   return (ret);
		 }
	       else
		 {
		   /*
		   ** was safeUse: shouldn't be safe!
		   **
		   ** to do rec->field
		   ** rec must be defined,
		   ** *rec must be allocated
		   ** rec->field need only be defined it if is an rvalue
		   */
		   
		   uentry_setUsed (fentry, exprNode_loc (ret));
		   ret->typ = uentry_getType (fentry);
		   
		   exprNode_checkUse (ret, s->sref, s->loc);
		   
		   /* exprNode_checkUse (ret, sRef_makePointer (s->sref), s->loc); */
		   ret->sref = sRef_makeArrow (s->sref, uentry_rawName (fentry));
		   return (ret);
		 }
	     }
	   else /* Pointer to something that is not a struct or union*/
	     {
	       if (ctype_isRealAbstract (tr))
		 {
		   ctype xrt = ctype_forceRealType (tr);
		   
		   voptgenerror 
		     (FLG_ABSTRACT,
		      message ("Arrow access field of abstract type (%t): %s->%s", 
			       t, exprNode_unparse (s), f),
		      s->loc);
		   
		   /*
		   ** Set the state correctly, as if the abstraction is broken.
		   */
		   
		   if (ctype_isRealPointer (xrt) &&
		       (b = ctype_realType (ctype_baseArrayPtr (xrt)),
			ctype_isStructorUnion (b)))
		     {
		       uentry fentry = uentryList_lookupField (ctype_getFields (b), f);
		       ret->typ = uentry_getType (fentry);
		       ret->sref = sRef_makeArrow (s->sref, uentry_rawName (fentry));
		     }
		   else
		     {
		       ret->typ = ctype_unknown;
		       ret->sref = sRef_undefined;
		     }
		 }
	       else /* not a struct, union or abstract */
		 {
		   if (ctype_isUnknown (tr)) {
		     cstring sn = cstring_copy (f);
		     
		     DPRINTF (("Here: %s", exprNode_unparse (s)));
		     
		     exprNode_checkUse (ret, s->sref, s->loc);
		     exprNode_checkUse (ret, sRef_makePointer (s->sref), s->loc);
		     
		     cstring_markOwned (sn);
		     ret->sref = sRef_makeArrow (s->sref, sn);
		     
		     ret->kind = XPR_ARROW;
		     return (ret);
		   } else {
		     voptgenerror 
		       (FLG_TYPE,
			message ("Arrow access field of non-struct or union "
				 "pointer (%t): %s->%s",
				 t, exprNode_unparse (s), f),
			s->loc);
		     
		     ret->typ = ctype_unknown;
		     ret->sref = sRef_undefined;
		   }
		 }
	     }
	 }
       else /* its not a pointer */
	 {
	   if (!ctype_isUnknown (tr))
	     {
	       voptgenerror 
		 (FLG_TYPE,
		  message ("Arrow access of non-pointer (%t): %s->%s",
			   t, exprNode_unparse (s), f),
		  s->loc);
	       
	       ret->typ = ctype_unknown;
	       ret->sref = sRef_undefined;
	     }
	   else
	     {
	       cstring sn = cstring_copy (f);
	       
	       DPRINTF (("Here: %s", exprNode_unparse (s)));
	       
	       exprNode_checkUse (ret, s->sref, s->loc);
	       exprNode_checkUse (ret, sRef_makePointer (s->sref), s->loc);
	       
	       cstring_markOwned (sn);
	       ret->sref = sRef_makeArrow (s->sref, sn);
	       
	       ret->kind = XPR_ARROW;
	       return (ret);
	     }
	 }

      return (ret);
    }
  BADEXIT;
}

/*
** only postOp's in C: i++ and i--
*/

exprNode
exprNode_postOp (/*@only@*/ exprNode e, /*@only@*/ lltok op)
{
  /* check modification also */
  /* cstring opname = lltok_unparse (op);*/
  ctype t;
  exprNode ret = exprNode_createPartialCopy (e);

  ret->loc = fileloc_update (ret->loc, lltok_getLoc (op));
  ret->kind = XPR_POSTOP;
  ret->edata = exprData_makeUop (e, op);

  if (!exprNode_isDefined (e))
    {
      return ret;
    }

  checkMacroParen (e);

  exprNode_checkUse (ret, e->sref, e->loc);
  exprNode_checkSet (ret, e->sref);

  t = exprNode_getType (e);

  if (sRef_isUnsafe (e->sref))
    {
      voptgenerror (FLG_MACROPARAMS,
		    message ("Operand of %s is macro parameter (non-functional): %s%s", 
			     lltok_unparse (op), exprNode_unparse (e), lltok_unparse (op)),
		    e->loc);
      sRef_makeSafe (e->sref);
      sRef_makeSafe (ret->sref);
    }

  if (ctype_isForceRealNumeric (&t) || ctype_isRealAP (t))
    {
      ret->typ = e->typ;
    }
  else
    {
      if (ctype_isRealAbstract (t))
        {
          voptgenerror 
	    (FLG_ABSTRACT,
	     message ("Operand of %s is abstract type (%t): %s",
		      lltok_unparse (op), t, exprNode_unparse (e)),
	     e->loc);
        }
      else
        {
          voptgenerror 
	    (FLG_TYPE,
	     message ("Operand of %s is non-numeric (%t): %s",
		      lltok_unparse (op), t, exprNode_unparse (e)),
	     e->loc);
        }
      ret->typ = ctype_unknown;
    }

  /* if (ctype_isZero (t)) e->typ = ctype_int; */

  exprNode_checkModify (e, ret);

  return ret;
}

exprNode
exprNode_preOp (/*@only@*/ exprNode e, /*@only@*/ lltok op)
{
  bool checkMod = FALSE;
  ctype te, tr;
  int opid = lltok_getTok (op);
  exprNode ret = exprNode_createSemiCopy (e);

  exprNode_copySets (ret, e);

  multiVal_free (ret->val);
  ret->val = multiVal_undefined;
  ret->loc = fileloc_update (ret->loc, lltok_getLoc (op));
  ret->kind = XPR_PREOP;  
  ret->edata = exprData_makeUop (e, op);
  
  if (exprNode_isError (e))
    {
      return ret;
    }
  
  checkMacroParen (e);
  
  te = exprNode_getType (e);
  tr = ctype_realType (te);
  
  if (opid != TAMPERSAND)
    {
      exprNode_checkUse (ret, e->sref, e->loc);
      
      if (ctype_isRealAbstract (tr)
	  && (!(ctype_isRealBool (te) && (opid == TEXCL))))
	{
	  if (optgenerror (FLG_ABSTRACT,
			   message ("Operand of %s is abstract type (%t): %s",
				    lltok_unparse (op), tr,
				    exprNode_unparse (ret)),
			   e->loc))
	    {
	      tr = te = ctype_unknown;
	      ret->typ = ctype_unknown;
	      sRef_setNullError (e->sref);
	    }
	}
    }
  
  switch (opid)
    {
    case INC_OP:
    case DEC_OP:		/* should also check modification! */

      if (sRef_isMacroParamRef (e->sref))
	{
	  voptgenerror 
	    (FLG_MACROPARAMS,
	     message ("Operand of %s is macro parameter (non-functional): %s", 
		      lltok_unparse (op), exprNode_unparse (ret)),
	     e->loc);
	}
      else
	{
	  exprNode_checkSet (ret, e->sref);
	}
      
      if (ctype_isForceRealNumeric (&tr) || ctype_isRealAP (tr))
	{
	}
      else
	{
	  if (context_msgStrictOps ())
	    {
	      voptgenerror 
		(FLG_STRICTOPS,
		 message ("Operand of %s is non-numeric (%t): %s",
			  lltok_unparse (op), te, exprNode_unparse (ret)),
		 e->loc);
	    }
	  ret->typ = ctype_int;
	}
      
      checkMod = TRUE;
      break;
      
    case TMINUS:
    case TPLUS:
      if (ctype_isForceRealNumeric (&tr))
	{
	  if (opid == TMINUS)
	    {
	      ret->val = multiVal_invert (exprNode_getValue (e));
	    }
	  else
	    {
	      ret->val = multiVal_copy (exprNode_getValue (e));
	    }
	}
      else
	{
	  if (context_msgStrictOps ())
    	    {
	      voptgenerror 
		(FLG_STRICTOPS,
		 message ("Operand of %s is non-numeric (%t): %s",
			  lltok_unparse (op), te, exprNode_unparse (ret)),
		 e->loc);
	    }

	  ret->typ = ctype_int;
	}
      break;
      
    case TEXCL:		/* maybe this should be restricted */
      guardSet_flip (ret->guards);      

      if (ctype_isRealBool (te))
	{
	 ;
	}
      else
	{
	  if (ctype_isRealPointer (tr))
	    {
	      if (sRef_isKnown (e->sref))
		{
		  ret->guards = guardSet_addFalseGuard (ret->guards, e->sref);
		}

	      voptgenerror2n
		(FLG_BOOLOPS, FLG_PTRNEGATE,
		 message ("Operand of %s is non-boolean (%t): %s",
			  lltok_unparse (op), te, exprNode_unparse (ret)),
		 e->loc);
	    }
	  else
	    {
	      voptgenerror
		(FLG_BOOLOPS,
		 message ("Operand of %s is non-boolean (%t): %s",
			  lltok_unparse (op), te, exprNode_unparse (ret)),
		 e->loc);
	    }
	  
	  ret->typ = ctype_bool;
	}
      break;
      
    case TTILDE:
      if (ctype_isForceRealInt (&tr))
	{
	}
      else
	{
	  if (context_msgStrictOps ())
	    {
	      voptgenerror 
		(FLG_STRICTOPS,
		 message ("Operand of %s is non-integer (%t): %s",
			  lltok_unparse (op), te, exprNode_unparse (ret)), 
		 e->loc);
	    }

	  if (ctype_isInt (e->typ))
	    {
	      ret->typ = e->typ;
	    }
	  else
	    {
	      ret->typ = ctype_int;
	    }
	}	
      break;
      
    case TAMPERSAND:
      ret->typ = ctype_makePointer (e->typ);

      if (sRef_isKnown (e->sref))
	{
	  ret->sref = sRef_makeAddress (e->sref);
	}
      
      break;
      
    case TMULT:
      
      if (ctype_isAP (tr))
	{
	  ret->typ = ctype_baseArrayPtr (e->typ);
	}
      else
	{
	  if (ctype_isKnown (te))
	    {
	      if (ctype_isFunction (te))
		{
		  ret->typ = e->typ;

		  voptgenerror
		    (FLG_FCNDEREF,
		     message ("Dereference of function type (%t): %s",
			      te, exprNode_unparse (ret)),
		     e->loc);
		}
	      else
		{
		  voptgenerror (FLG_TYPE,
				message ("Dereference of non-pointer (%t): %s",
					 te, exprNode_unparse (ret)),
				e->loc);
		  ret->typ = ctype_unknown;
		}
	    }
	  else
	    {
	      ret->typ = ctype_unknown;
	    }
	  
	}
      
      if (sRef_isKnown (e->sref))
	{
	  if (sRef_possiblyNull (e->sref))
	    {
	      if (!usymtab_isGuarded (e->sref) && !context_inProtectVars ())
		{
		  if (optgenerror 
		      (FLG_NULLDEREF,
		       message ("Dereference of %s pointer %q: %s", 
				sRef_nullMessage (e->sref),
				sRef_unparse (e->sref),
				exprNode_unparse (ret)),
		       e->loc))
		    {
		      sRef_showNullInfo (e->sref);
		      sRef_setNotNull (e->sref, e->loc); /* suppress future messages */
		    }
		}
	    }
	  
	  ret->sref = sRef_makePointer (e->sref);
	}
      break;
      
    default:
      llbug (message ("exprNode_preOp: unhandled op: %s", lltok_unparse (op)));
    }

  if (checkMod)
    {
      exprNode_checkModify (e, ret);
    }

  return ret;
}

/*
** any reason to disallow sizeof (abstract type) ?
*/

/*
** used by both sizeof
*/

static
ctype sizeof_resultType (void)
{
  static ctype sizet = ctype_unknown;

  if (ctype_isUnknown (sizet))
    {
      if (usymtab_existsType (cstring_makeLiteralTemp ("size_t")))
	{
	  sizet = uentry_getAbstractType (usymtab_lookup (cstring_makeLiteralTemp ("size_t")));
	}
      else
	{
	  	  sizet = ctype_ulint;
	}
    }
  return sizet;
}

exprNode
exprNode_sizeofType (/*@only@*/ qtype qt)
{
  exprNode ret = exprNode_create (sizeof_resultType ());
  ctype ct = qtype_getType (qt);

  ret->kind = XPR_SIZEOFT;
  ret->edata = exprData_makeSizeofType (qt);

  voptgenerror (FLG_SIZEOFTYPE,
		message ("Parameter to sizeof is type %s: %s",
			 ctype_unparse (ct),
			 exprNode_unparse (ret)),
		ret->loc);
  
  return (ret);
}

exprNode
exprNode_alignofType (/*@only@*/ qtype qt)
{
  exprNode ret = exprNode_create (sizeof_resultType ());
  ctype ct = qtype_getType (qt);

  ret->kind = XPR_ALIGNOFT;
  ret->edata = exprData_makeSizeofType (qt);

  voptgenerror (FLG_SIZEOFTYPE,
		message ("Parameter to alignof is type %s: %s",
			 ctype_unparse (ct),
			 exprNode_unparse (ret)),
		ret->loc);
  
  return (ret);
}

exprNode exprNode_offsetof (qtype qt, cstringList s)
{
  exprNode ret = exprNode_create (sizeof_resultType ());
  ctype ct = qtype_getType (qt);

  ret->kind = XPR_OFFSETOF;
  ret->edata = exprData_makeOffsetof (qt, s);

  if (!ctype_isRealSU (ct))
    {
      voptgenerror (FLG_TYPE,
		    message ("First parameter to offsetof is not a "
			     "struct or union type (type %s): %s",
			     ctype_unparse (ct),
			     exprNode_unparse (ret)),
		    ret->loc);
    }
  else
    {
      ctype lt = ct;

      cstringList_elements (s, el) {
	uentryList fields;
	uentry fld;

	if (ctype_isUndefined (lt))
	  {
	    break;
	  } 
	else if (!ctype_isRealSU (lt))
	  {
	    voptgenerror (FLG_TYPE,
			  message ("Inner offsetof type is not a "
				   "struct or union type (type %s before field %s): %s",
				   ctype_unparse (lt), el,
				   exprNode_unparse (ret)),
			  ret->loc);
	    break;
	  }
	else 
	  {
	    fields = ctype_getFields (ctype_realType (lt));
	    fld = uentryList_lookupField (fields, el);
	    DPRINTF (("Try: %s / %s", ctype_unparse (lt), el));
	    
	    if (uentry_isUndefined (fld))
	      {
		if (ctype_equal (lt, ct)) {
		  voptgenerror (FLG_TYPE,
				message ("Field %s in offsetof is not the "
					 "name of a field of %s: %s",
					 el,
					 ctype_unparse (ct),
					 exprNode_unparse (ret)),
				ret->loc);
		} else {
		  voptgenerror (FLG_TYPE,
				message ("Deep field %s in offsetof is not the "
					 "name of a field of %s: %s",
					 el,
					 ctype_unparse (lt),
					 exprNode_unparse (ret)),
				ret->loc);
		}
	      }
	    else 
	      {
		lt = uentry_getType (fld);
	      }
	  }
      } end_cstringList_elements;

      /* Should report error if its a bit field - behavior is undefined! */
    }
  
  return (ret);
}

/*@only@*/ exprNode
exprNode_sizeofExpr (/*@only@*/ exprNode e)
{
  exprNode ret;

  if (exprNode_isUndefined (e))
    {
      ret = exprNode_createLoc (ctype_unknown, fileloc_copy (g_currentloc));
      ret->edata = exprData_makeSingle (e);
      ret->typ = sizeof_resultType ();
      ret->kind = XPR_SIZEOF;
    }
  else
    {
      uentry u = exprNode_getUentry (e);

      ret = exprNode_createPartialCopy (e);
      ret->edata = exprData_makeSingle (e);

      ret->typ = sizeof_resultType ();
      ret->kind = XPR_SIZEOF;

      if (uentry_isValid (u) 
	  && uentry_isRefParam (u)
	  && ctype_isRealArray (uentry_getType (u)))
	{
	  voptgenerror
	    (FLG_SIZEOFFORMALARRAY,
	     message ("Parameter to sizeof is an array-type function parameter: %s",
		      exprNode_unparse (ret)),
	     ret->loc);
	}
    }

  /*
  ** sizeof (x) doesn't "really" use x
  */

  return (ret);
}

/*@only@*/ exprNode
exprNode_alignofExpr (/*@only@*/ exprNode e)
{
  exprNode ret;

  if (exprNode_isUndefined (e))
    {
      ret = exprNode_createLoc (ctype_unknown, fileloc_copy (g_currentloc));
    }
  else
    {
      ret = exprNode_createPartialCopy (e);
    }

  ret->edata = exprData_makeSingle (e);
  ret->typ = sizeof_resultType ();
  ret->kind = XPR_ALIGNOF;
  
  /*
  ** sizeof (x) doesn't "really" use x
  */

  return (ret);
}

/*@only@*/ exprNode
exprNode_cast (/*@only@*/ lltok tok, /*@only@*/ exprNode e, /*@only@*/ qtype q)
{
  ctype c;
  ctype t;
  exprNode ret;

  if (exprNode_isError (e))
    {
      qtype_free (q);
      lltok_release (tok);
      return exprNode_undefined;
    }

  checkMacroParen (e);

  c = qtype_getType (q);
  t = exprNode_getType (e);

  ret = exprNode_createPartialCopy (e);
  
  ret->loc = fileloc_update (ret->loc, lltok_getLoc (tok));
  ret->typ = c;
  ret->kind = XPR_CAST;
  ret->edata = exprData_makeCast (tok, e, q);

  if (ctype_isRealSU (ctype_getBaseType (sRef_getType (e->sref))))
    {
      /* 
      ** This is a bit of a hack to avoid a problem
      ** when the code does,
      **          (some other struct) x
      **          ...
      **          x->field
      */

      ret->sref = sRef_copy (e->sref);
      usymtab_addForceMustAlias (ret->sref, e->sref);
      sRef_setTypeFull (ret->sref, c);
      DPRINTF (("Cast: %s -> %s", sRef_unparseFull (e->sref),
		sRef_unparseFull (ret->sref)));
    }
  else
    {
      ret->sref = e->sref;
      sRef_setTypeFull (ret->sref, c);
      DPRINTF (("Cast 2: -> %s", sRef_unparseFull (ret->sref)));
    }

  /*
  ** we allow
  **       abstract  -> void
  **              0 <-> abstract * 
  **         void * <-> abstract *  (if FLG_ABSTVOIDP)
  **     abstract * <-> void *      (if FLG_ABSTVOIDP)
  */

  if (ctype_isVoid (c)) /* cast to void is always okay --- discard value */
    {
      ;
    }
  else if (ctype_isRealAP (c)) /* casting to array or pointer */
    {
      ctype bc = ctype_getBaseType (c);
      ctype bt = ctype_getBaseType (t);
      ctype rt = ctype_realType (t);

      if (ctype_isFunction (ctype_baseArrayPtr (ctype_realType (c)))
	  && (ctype_isArrayPtr (rt)
	      && !ctype_isFunction (ctype_realType (ctype_baseArrayPtr (rt)))))
	{
	  voptgenerror
	    (FLG_CASTFCNPTR,
	     message ("Cast from function pointer type (%t) to "
		      "non-function pointer (%t): %s",
		      c, t, exprNode_unparse (ret)),
	     e->loc);	  
	}

      if (!ctype_isFunction (ctype_baseArrayPtr (c))
	  && (ctype_isArrayPtr (rt)
	      && ctype_isFunction (ctype_realType (ctype_baseArrayPtr (rt)))))
	{
	  voptgenerror
	    (FLG_CASTFCNPTR,
	     message ("Cast from non-function pointer type (%t) to "
		      "function pointer (%t): %s",
		      c, t, exprNode_unparse (ret)),
	     e->loc);	  
	}

      if (exprNode_isZero (e) && context_getFlag (FLG_ZEROPTR) &&
	  !(ctype_isRealAbstract (bc)
	    && context_hasAccess (ctype_typeId (bc))))
	{
	 ; /* okay to cast zero */
	}
      else
	{
	  if (ctype_isRealAbstract (bc)
	      && !context_hasAccess (ctype_typeId (bc)))
	    {
	      if (ctype_isVoidPointer (t) || ctype_isUnknown (t))
		{
		  vnoptgenerror
		    (FLG_ABSTVOIDP,
		     message ("Cast to underlying abstract type %t: %s",
			      c, exprNode_unparse (ret)),
		     e->loc);
		}
	      else
		{
		  voptgenerror
		    (FLG_ABSTRACT,
		     message ("Cast to underlying abstract type %t: %s",
			      c, exprNode_unparse (ret)),
		     e->loc);	  
		}
	    }

	  if (ctype_isRealAbstract (bt)
	      && !context_hasAccess (ctype_typeId (bt)))
	    {
	      if (ctype_isUnknown (c) || ctype_isVoidPointer (c))
		{
		  vnoptgenerror
		    (FLG_ABSTVOIDP,
		     message ("Cast from underlying abstract type %t: %s",
			      t, exprNode_unparse (ret)),
		     e->loc);
		}
	      else
		{
		  voptgenerror
		    (FLG_ABSTRACT,
		     message ("Cast from underlying abstract type %t: %s",
			      t, exprNode_unparse (ret)),
		     e->loc);
		}
	    }
	}
    }
  else
    {
      ctype bt = ctype_realType (ctype_getBaseType (t));
      ctype bc = ctype_realType (ctype_getBaseType (c));

      if (ctype_isAbstract (bt) && !context_hasAccess (ctype_typeId (bt)))
	{
	  if (ctype_match (c, t))
	    {
	      if (ctype_equal (c, t))
		{
		  voptgenerror
		    (FLG_TYPE, 
		     message ("Redundant cast involving abstract type %t: %s",
			      bt, exprNode_unparse (ret)),
		     e->loc);
		}
	    }
	  else
	    {
	      voptgenerror
		(FLG_ABSTRACT,
		 message ("Cast from abstract type %t: %s", 
			  bt, exprNode_unparse (ret)),
		 e->loc);
	    }
	}
      
      if (ctype_isAbstract (bc) 
	  && !context_hasAccess (ctype_typeId (bc)))
	{
	  if (ctype_match (c, t))
	    {
	     ;
	    }
	  else
	    {
	      voptgenerror 
		(FLG_ABSTRACT,
		 message ("Cast to abstract type %t: %s", bc, 
			  exprNode_unparse (ret)),
		 e->loc);
	    }
	}
    }

  if (ctype_isAbstract (c))
    {
      if (sRef_isExposed (e->sref) || sRef_isOnly (e->sref))
	{
	  /* okay, cast exposed to abstract */
	  sRef_clearExKindComplete (ret->sref, fileloc_undefined);
	}
      else 
	{
	  if (ctype_isVisiblySharable (t) 
	      && sRef_isExternallyVisible (e->sref)
	      && !(ctype_isAbstract (t) 
		   && context_hasAccess (ctype_typeId (t))))
	    {
	      voptgenerror 
		(FLG_CASTEXPOSE,
		 message ("Cast to abstract type from externally visible "
			  "mutable storage exposes rep of %s: %s",
			  ctype_unparse (c),
			  exprNode_unparse (e)),
		 e->loc);
	    }
	}
    }

  return (ret);
}

static bool
evaluationOrderUndefined (lltok op)
{
  int opid = lltok_getTok (op);

  return (opid != AND_OP && opid != OR_OP);
}

static bool checkIntegral (/*@notnull@*/ exprNode e1, 
			   /*@notnull@*/ exprNode e2, 
			   /*@notnull@*/ exprNode ret, 
			   lltok op)
{
  bool error = FALSE;

  ctype te1 = exprNode_getType (e1);
  ctype te2 = exprNode_getType (e2);

  ctype tr1 = ctype_realishType (te1);
  ctype tr2 = ctype_realishType (te2);
  
  if (ctype_isForceRealInt (&tr1) && ctype_isForceRealInt (&tr2))
    {
      ;
    }
  else
    {
      if (context_msgStrictOps ())
	{
	  if (!ctype_isInt (tr1) && !ctype_isInt (tr2))
	    {
	      if (ctype_sameName (te1, te2))
		{
		  error = optgenerror 
		    (FLG_STRICTOPS,
		     message ("Operands of %s are non-integer (%t): %s",
			      lltok_unparse (op), te1,
			      exprNode_unparse (ret)),
		     e1->loc);
		}
	      else
		{
		  error = optgenerror 
		    (FLG_STRICTOPS,
		     message ("Operands of %s are non-integers (%t, %t): %s",
			      lltok_unparse (op), te1, te2,
			      exprNode_unparse (ret)),
		     e1->loc);
		}
	    }
	  else if (!ctype_isInt (tr1))
	    {
	      error = optgenerror 
		(FLG_STRICTOPS,
		 message ("Left operand of %s is non-integer (%t): %s",
			  lltok_unparse (op), te1, exprNode_unparse (ret)),
		 e1->loc);
	    }
	  else
	    /* !ctype_isInt (te2) */
	    {
	      error = optgenerror 
		(FLG_STRICTOPS,
		 message ("Right operand of %s is non-integer (%t): %s",
			  lltok_unparse (op), te2, exprNode_unparse (ret)),
		 e2->loc);
	    }
	}
    }

  return !error;
}

/*
** returns exprNode representing e1 op e2
**
** uses msg if there are errors
** can be used for both assignment ops and regular ops
**
** modifies e1
*/

static /*@only@*/ /*@notnull@*/ exprNode
exprNode_makeOp (/*@keep@*/ exprNode e1, /*@keep@*/ exprNode e2, 
		 /*@keep@*/ lltok op)
{
  ctype te1, te2, tr1, tr2, tret;
  int opid = lltok_getTok (op);
  bool hasError = FALSE;
  exprNode ret;

  if (exprNode_isError (e1))
    {
      ret = exprNode_createPartialNVCopy (e2);
    }
  else
    {
      ret = exprNode_createPartialNVCopy (e1);    
    }

  ret->val = multiVal_undefined;
  ret->kind = XPR_OP;
  ret->edata = exprData_makeOp (e1, e2, op);

  if (exprNode_isError (e1) || exprNode_isError (e2))
    {
      if (opid == TLT || opid == TGT || opid == LE_OP || opid == GE_OP
	  || opid == EQ_OP || opid == NE_OP 
	  || opid == AND_OP || opid == OR_OP)
	{
	  ret->typ = ctype_bool;
	}

      if (exprNode_isDefined (e1))
	{
	  exprNode_checkUse (ret, e1->sref, e1->loc);  
	}

      if (exprNode_isDefined (e2))
	{
	  exprNode_mergeUSs (ret, e2);
	  exprNode_checkUse (ret, e2->sref, e2->loc);  
	}

      return ret;
    }

  tret = ctype_unknown;
  te1 = exprNode_getType (e1);
  DPRINTF (("te1 = %s / %s", exprNode_unparse (e1), ctype_unparse (te1)));

  te2 = exprNode_getType (e2);

  tr1 = ctype_realishType (te1);
  tr2 = ctype_realishType (te2);

  if (opid == OR_OP)
    {
      ret->guards = guardSet_or (ret->guards, e2->guards);
    }
  else if (opid == AND_OP)
    {
      ret->guards = guardSet_and (ret->guards, e2->guards);
    }
  else
    {
      /* no guards */
    }

  if (opid == EQ_OP || opid == NE_OP)
    {
      exprNode temp1 = e1, temp2 = e2;

      /* could do NULL == x */
      
      if (exprNode_isNullValue (e1) || exprNode_isUnknownConstant (e1))
	{
	  temp1 = e2; temp2 = e1;
	}

      if (exprNode_isNullValue (temp2) || exprNode_isUnknownConstant (temp2))
	{
	  reflectNullTest (temp1, (opid == NE_OP));
	  guardSet_free (ret->guards);
	  ret->guards = guardSet_copy (temp1->guards);
	}
    }

  if (opid == TLT || opid == TGT || opid == LE_OP || opid == GE_OP
      || opid == EQ_OP || opid == NE_OP || opid == AND_OP || opid == OR_OP)
    {
      tret = ctype_bool; 
    }
  
  if (anyAbstract (tr1, tr2) &&
      (!((ctype_isRealBool (te1) || ctype_isRealBool (te2)) && 
	 (opid == AND_OP || opid == OR_OP 
	  || opid == EQ_OP || opid == NE_OP))))
    {
      abstractOpError (tr1, tr2, op, e1, e2, e1->loc, e2->loc);
    }
  else if (ctype_isUnknown (te1) || ctype_isUnknown (te2))
    {
      /* unknown types, no comparisons possible */
    }
  else
    {
      switch (opid)
	{
	case TMULT:		/* multiplication and division:           */
	case TDIV:		/*                                        */
	case MUL_ASSIGN:	/*    numeric, numeric -> numeric         */
	case DIV_ASSIGN:	/*                                        */
	  
	  tret = checkNumerics (tr1, tr2, te1, te2, e1, e2, op);
	  break;
	  
	case TPLUS:		/* addition and subtraction:               */
	case TMINUS:		/*    pointer, int     -> pointer          */
	case SUB_ASSIGN:	/*    int, pointer     -> pointer          */
	case ADD_ASSIGN:	/*    numeric, numeric -> numeric          */
	  
	  tr1 = ctype_fixArrayPtr (tr1);

	  if ((ctype_isRealPointer (tr1) && !exprNode_isNullValue (e1))
	      && (!ctype_isRealPointer (tr2) && ctype_isRealInt (tr2)))
	    {
	      /* pointer + int */

	      if (context_msgPointerArith ())
		{
		  voptgenerror
		    (FLG_POINTERARITH,
		     message ("Pointer arithmetic (%t, %t): %s", 
			      te1, te2, exprNode_unparse (ret)),
		     e1->loc);
		}

	      if (sRef_possiblyNull (e1->sref)
		  && !usymtab_isGuarded (e1->sref))
		{
		  voptgenerror
		    (FLG_NULLPOINTERARITH,
		     message ("Pointer arithmetic involving possibly "
			      "null pointer %s: %s", 
			      exprNode_unparse (e1), 
			      exprNode_unparse (ret)),
		     e1->loc);
		}

	      ret->sref = sRef_copy (e1->sref);

	      sRef_setNullError (ret->sref);

	      /*
	      ** Fixed for 2.2c: the alias state of ptr + int is dependent,
	      ** since is points to storage that should not be deallocated
	      ** through this pointer.
	      */

	      if (sRef_isOnly (ret->sref) 
		  || sRef_isFresh (ret->sref)) 
		{
		  sRef_setAliasKind (ret->sref, AK_DEPENDENT, exprNode_loc (ret));
		}
	      
	      tret = e1->typ;
	    }
	  else if ((!ctype_isRealPointer(tr1) && ctype_isRealInt (tr1)) 
		   && (ctype_isRealPointer (tr2) && !exprNode_isNullValue (e2)))
	    {
	      if (context_msgPointerArith ())
		{
		  voptgenerror 
		    (FLG_POINTERARITH,
		     message ("Pointer arithmetic (%t, %t): %s", 
			      te1, te2, exprNode_unparse (ret)),
		     e1->loc);
		}

	      if (sRef_possiblyNull (e1->sref)
		  && !usymtab_isGuarded (e1->sref))
		{
		  voptgenerror
		    (FLG_NULLPOINTERARITH,
		     message ("Pointer arithmetic involving possibly "
			      "null pointer %s: %s", 
			      exprNode_unparse (e2), 
			      exprNode_unparse (ret)),
		     e2->loc);
		}

	      ret->sref = sRef_copy (e2->sref);

	      sRef_setNullError (ret->sref);

	      /*
	      ** Fixed for 2.2c: the alias state of ptr + int is dependent,
	      ** since is points to storage that should not be deallocated
	      ** through this pointer.
	      */

	      if (sRef_isOnly (ret->sref) 
		  || sRef_isFresh (ret->sref)) {
		sRef_setAliasKind (ret->sref, AK_DEPENDENT, exprNode_loc (ret));
	      }

	      tret = e2->typ;
	      ret->sref = e2->sref;
	    }
	  else
	    {
	      tret = checkNumerics (tr1, tr2, te1, te2, e1, e2, op);
	    }
	  
	  break;

	case LEFT_ASSIGN:   /* Shifts: should be unsigned values */
	case RIGHT_ASSIGN:
	case LEFT_OP:
	case RIGHT_OP:
	case TAMPERSAND:    /* bitwise & */
	case AND_ASSIGN:       
	case TCIRC:         /* ^ (XOR) */
	case TBAR:
	case XOR_ASSIGN:
	case OR_ASSIGN:
	  {
	    bool reported = FALSE;
	    flagcode code = FLG_BITWISEOPS;
	    
	    if (opid == LEFT_OP || opid == LEFT_ASSIGN
		|| opid == RIGHT_OP || opid == RIGHT_ASSIGN) {
	      code = FLG_SHIFTSIGNED;
	    }

	    if (!ctype_isUnsigned (tr1)) 
	      {
		if (exprNode_isNonNegative (e1)) {
		  ;
		} else {
		  reported = optgenerror 
		    (code,
		     message ("Left operand of %s is not unsigned value (%t): %s",
			      lltok_unparse (op), te1,
			      exprNode_unparse (ret)),
		     e1->loc);
		  
		  if (reported) {
		    te1 = ctype_uint;
		  }
		}
	      }
	    else 
	      {
		/* right need not be signed for shifts */
		if (code != FLG_SHIFTSIGNED 
		    && !ctype_isUnsigned (tr2)) 
		  {
		    if (!exprNode_isNonNegative (e2)) {
		      reported = optgenerror 
			(code,
			 message ("Right operand of %s is not unsigned value (%t): %s",
				  lltok_unparse (op), te2,
				  exprNode_unparse (ret)),
			 e2->loc);
		    }
		  }
	      }
	    
	    if (!reported) 
	      {
		if (!checkIntegral (e1, e2, ret, op)) {
		  te1 = ctype_unknown;
		}
	      }
	    
	    DPRINTF (("Set: %s", ctype_unparse (te1)));	    

	    /*
	    ** tret is the widest type of te1 and te2 
	    */

	    tret = ctype_widest (te1, te2);
	    break;
	  }
	case MOD_ASSIGN:
	case TPERCENT:		
	  if (checkIntegral (e1, e2, ret, op)) {
	    tret = te1;
	  } else {
	    tret = ctype_unknown;
	  }
	  break;
	case EQ_OP: 
	case NE_OP:
	case TLT:		/* comparisons                           */
	case TGT:		/*    numeric, numeric -> bool           */
	  if ((ctype_isReal (tr1) && !ctype_isInt (tr1))
	      || (ctype_isReal (tr2) && !ctype_isInt (tr2)))
	    {
	      ctype rtype = tr1;
	      bool fepsilon = FALSE;

	      if (!ctype_isReal (rtype) || ctype_isInt (rtype))
		{
		  rtype = tr2;
		}
	      
	      if (opid == TLT || opid == TGT)
		{
		  uentry ue1 = exprNode_getUentry (e1);
		  uentry ue2 = exprNode_getUentry (e2);

		  /*
		  ** FLT_EPSILON, etc. really is a variable, not
		  ** a constant.
		  */

		  if (uentry_isVariable (ue1))
		    {
		      cstring uname = uentry_rawName (ue1);

		      if (cstring_equalLit (uname, "FLT_EPSILON")
			  || cstring_equalLit (uname, "DBL_EPSILON")
			  || cstring_equalLit (uname, "LDBL_EPSILON"))
			{
			  fepsilon = TRUE;
			}
		    }

		  if (uentry_isVariable (ue2))
		    {
		      cstring uname = uentry_rawName (ue2);

		      if (cstring_equalLit (uname, "FLT_EPSILON")
			  || cstring_equalLit (uname, "DBL_EPSILON")
			  || cstring_equalLit (uname, "LDBL_EPSILON"))
			{
			  fepsilon = TRUE;
			}
		    }
		}

	      if (fepsilon)
		{
		  ; /* Don't complain. */
		}
	      else
		{
		  voptgenerror
		    (FLG_REALCOMPARE,
		     message ("Dangerous comparison involving %s types: %s",
			      ctype_unparse (rtype),
			      exprNode_unparse (ret)),
		     ret->loc);
		}
	    }
	  /*@fallthrough@*/
	case LE_OP:
	case GE_OP:

	  /*
	  ** Types should match.
	  */

	  if (!exprNode_matchTypes (e1, e2))
	    {
	      hasError = gentypeerror 
		(te1, e1, te2, e2,
		 message ("Operands of %s have incompatible types (%t, %t): %s",
			  lltok_unparse (op), te1, te2, exprNode_unparse (ret)),
		 e1->loc);
	      
	    }

	  if (hasError 
	      || (ctype_isForceRealNumeric (&tr1)
		  && ctype_isForceRealNumeric (&tr2)) ||
	      (ctype_isRealPointer (tr1) && ctype_isRealPointer (tr2)))
	    {
	      ; /* okay */
	    }
	  else
	    {
	      if ((ctype_isRealNumeric (tr1) && ctype_isRealPointer (tr2)) ||
		  (ctype_isRealPointer (tr1) && ctype_isRealNumeric (tr2)))
		{
		  voptgenerror 
		    (FLG_PTRNUMCOMPARE,
		     message ("Comparison of pointer and numeric (%t, %t): %s",
			      te1, te2, exprNode_unparse (ret)),
		     e1->loc);
		}
	      else
		{
		  (void) checkNumerics (tr1, tr2, te1, te2, e1, e2, op);
		}
	      tret = ctype_bool;
	    }

	  /* EQ_OP should NOT be used with booleans (unless one is FALSE) */
	  
	  if ((opid == EQ_OP || opid == NE_OP) && 
	      ctype_isDirectBool (tr1) && ctype_isDirectBool (tr2))
	    {
	      /*
	      ** is one a variable?
	      */

	      if (uentry_isVariable (exprNode_getUentry (e1))
		  || uentry_isVariable (exprNode_getUentry (e2)))
		{
		  /*
		  ** comparisons with FALSE are okay
		  */

		  if (exprNode_isFalseConstant (e1)
		      || exprNode_isFalseConstant (e2))
		    {
		      ;
		    }
		  else
		    {
		      voptgenerror
			(FLG_BOOLCOMPARE,
			 message 
			 ("Use of %q with %s variables (risks inconsistency because "
			  "of multiple true values): %s",
			  cstring_makeLiteral ((opid == EQ_OP) ? "==" : "!="),
			  context_printBoolName (), exprNode_unparse (ret)),
			 e1->loc);
		    }
		}
	    }
	  break;
	  
	case AND_OP:		/* bool, bool -> bool */
	case OR_OP:

	  if (ctype_isForceRealBool (&tr1) && ctype_isForceRealBool (&tr2))
	    {
	      ; 
	    }
	  else
	    {
	      if (context_maybeSet (FLG_BOOLOPS))
		{
		  if (!ctype_isRealBool (te1) && !ctype_isRealBool (te2))
		    {
		      if (ctype_sameName (te1, te2))
			{
			  voptgenerror 
			    (FLG_BOOLOPS,
			     message ("Operands of %s are non-boolean (%t): %s",
				      lltok_unparse (op), te1,
				      exprNode_unparse (ret)),
			     e1->loc);
			}
		      else
			{
			  voptgenerror 
			    (FLG_BOOLOPS,
			     message
			     ("Operands of %s are non-booleans (%t, %t): %s",
			      lltok_unparse (op), te1, te2, exprNode_unparse (ret)),
			     e1->loc);
			}
		    }
		  else if (!ctype_isRealBool (te1))
		    {
		      voptgenerror 
			(FLG_BOOLOPS,
			 message ("Left operand of %s is non-boolean (%t): %s",
				  lltok_unparse (op), te1, exprNode_unparse (ret)),
			 e1->loc);
		    }
		  else if (!ctype_isRealBool (te2))
		    {
		      voptgenerror
			(FLG_BOOLOPS,
			 message ("Right operand of %s is non-boolean (%t): %s",
				  lltok_unparse (op), te2, exprNode_unparse (ret)),
			 e2->loc);
		    }
		  else
		    {
		      ;
		    }
		}
	      tret = ctype_bool;
	    }
	  break;
	default: {
	    llfatalbug 
	      (cstring_makeLiteral 
	       ("There has been a problem in the parser. This usually results "
		"from using an old version of bison (1.25) to build LCLint. "
		"Please upgrade your bison implementation to version 1.28."));
	  }
	}
    }

  DPRINTF (("Return type: %s", ctype_unparse (tret)));
  ret->typ = tret;

  exprNode_checkUse (ret, e1->sref, e1->loc);  
  exprNode_mergeUSs (ret, e2);
  exprNode_checkUse (ret, e2->sref, e2->loc);  

  return ret;
}

/*@only@*/ exprNode
exprNode_op (/*@only@*/ exprNode e1, /*@keep@*/ exprNode e2,
	     /*@only@*/ lltok op)
{
  exprNode ret;

  checkMacroParen (e1);
  checkMacroParen (e2);

  if (evaluationOrderUndefined (op) && context_maybeSet (FLG_EVALORDER))
    {
      checkExpressionDefined (e1, e2, op);
    }

  ret = exprNode_makeOp (e1, e2, op);
  return (ret);
}

static
void exprNode_checkAssignMod (exprNode e1, exprNode ret)
{
  /*
  ** This is somewhat bogus!
  **
  ** Assigning to a nested observer in a non-observer datatype
  ** should not produce an error.
  */

  sRef ref = exprNode_getSref (e1);

  DPRINTF (("Check assign mod: %s",
	    sRef_unparseFull (ref)));

  if (sRef_isObserver (ref) 
      || ((sRef_isFileStatic (ref) || sRef_isGlobal (ref))
	  && ctype_isArray (ctype_realType (sRef_getType (ref)))))
    {
      sRef base = sRef_getBase (ref);

      if (sRef_isValid (base) && sRef_isObserver (base))
	{
	  exprNode_checkModify (e1, ret);
	}
      else
	{
	  exprNode_checkModifyVal (e1, ret);
	}
    }
  else
    {
      exprNode_checkModify (e1, ret);
    }
}

exprNode
exprNode_assign (/*@only@*/ exprNode e1,
		 /*@only@*/ exprNode e2, /*@only@*/ lltok op)
{
  bool isalloc = FALSE;
  bool isjustalloc = FALSE;
  exprNode ret;

  DPRINTF (("%s [%s] <- %s [%s]",
	    exprNode_unparse (e1),
	    ctype_unparse (e1->typ),
	    exprNode_unparse (e2),
	    ctype_unparse (e2->typ)));

  if (lltok_getTok (op) != TASSIGN) 
    {
      ret = exprNode_makeOp (e1, e2, op);
    } 
  else 
    {
      ret = exprNode_createPartialCopy (e1);
      ret->kind = XPR_ASSIGN;
      ret->edata = exprData_makeOp (e1, e2, op);

      if (!exprNode_isError (e2)) 
	{
	  ret->sets = sRefSet_union (ret->sets, e2->sets);
	  ret->msets = sRefSet_union (ret->msets, e2->msets);
	  ret->uses = sRefSet_union (ret->uses, e2->uses);
	}
    }

  checkExpressionDefined (e1, e2, op);

  if (exprNode_isError (e1))
    {
      if (!exprNode_isError (e2)) 
	{
	  ret->loc = fileloc_update (ret->loc, e2->loc);
	}
      else
	{
	  ret->loc = fileloc_update (ret->loc, g_currentloc);
	}
    }

  if (!exprNode_isError (e2))
    {
      checkMacroParen (e2);
    }

  if (exprNode_isDefined (e1))
    {
      if (sRef_isMacroParamRef (e1->sref))
	{
	  if (context_inIterDef ())
	    {
	      uentry ue = sRef_getUentry (e1->sref);
	      
	      if (uentry_isYield (ue))
		{
		  ;
		}
	      else
		{
		  if (fileloc_isDefined (e1->loc))
		    {
		      voptgenerror
			(FLG_MACROPARAMS,
			 message ("Assignment to non-yield iter parameter: %q", 
				  sRef_unparse (e1->sref)),
			 e1->loc);
		    }
		  else
		    {
		      voptgenerror 
			(FLG_MACROPARAMS,
			 message ("Assignment to non-yield iter parameter: %q", 
				  sRef_unparse (e1->sref)),
			 g_currentloc);
		    }
		}
	    }
	  else
	    {
	      if (fileloc_isDefined (e1->loc))
		{
		  voptgenerror
		    (FLG_MACROASSIGN,
		     message ("Assignment to macro parameter: %q", 
			      sRef_unparse (e1->sref)),
		     e1->loc);
		}
	      else
		{
		  voptgenerror 
		    (FLG_MACROASSIGN,
		     message ("Assignment to macro parameter: %q", 
			      sRef_unparse (e1->sref)),
		     g_currentloc);
		}
	    }
	}
      else
	{
	  exprNode_checkAssignMod (e1, ret);
	}

      if (exprNode_isDefined (e2))
	{
	  if (lltok_getTok (op) == TASSIGN) 
	    {
	      ctype te1 = exprNode_getType (e1);
	      ctype te2 = exprNode_getType (e2);
	      
	      if (!ctype_forceMatch (te1, te2))
		{
		  if (exprNode_matchLiteral (te1, e2))
		    {
		      ;
		    }
		  else
		    {
		      (void) gentypeerror 
			(te2, e2, te1, e1,
			 message ("Assignment of %t to %t: %s %s %s", 
				  te2, te1, exprNode_unparse (e1),
				  lltok_unparse (op), 
				  exprNode_unparse (e2)),
			 e1->loc);
		    }
		}
	    }
	 
	  exprNode_mergeUSs (ret, e2);
	  exprNode_checkUse (ret, e2->sref, e2->loc);
	  
	  doAssign (e1, e2, FALSE); 
	  ret->sref = e1->sref;
	}
      else
	{
	  if (exprNode_isDefined (e2))
	    {
	      exprNode_mergeUSs (ret, e2);
	      	      exprNode_checkUse (ret, e2->sref, e2->loc);
	    }
	}

      if (sRef_isPointer (e1->sref) && !sRef_isMacroParamRef (e1->sref))
	{
	  exprNode_checkUse (ret, sRef_getBase (e1->sref), e1->loc);
	}

      isjustalloc = sRef_isJustAllocated (e1->sref);
      isalloc = sRef_isAllocated (e1->sref);

      if (sRef_isField (e1->sref))
	{
	  sRef root = sRef_getRootBase (sRef_getBase (e1->sref));
	  
	  if (!sRef_isAllocated (root) && !sRef_isMacroParamRef (root))
	    {
	      exprNode_checkUse (ret, root, e1->loc);
	    }
	  
	}
  
      /*
      ** be careful!  this defines e1->sref.
      */
   
      if (!sRef_isMacroParamRef (e1->sref))
	{
	  exprNode_checkSet (ret, e1->sref);
	}
      
      if (isjustalloc) 
	{
	  sRef_setAllocatedComplete (e1->sref, exprNode_isDefined (e2)
				     ? e2->loc : e1->loc);
	}
      else 
	{
	  if (isalloc)
	    {
	      sRef_setAllocatedShallowComplete (e1->sref, exprNode_loc (e2));
	    }
	}
    }
  
  return ret;
}

exprNode
exprNode_cond (/*@keep@*/ exprNode pred, /*@keep@*/ exprNode ifclause, 
	       /*@keep@*/ exprNode elseclause)
{
  exprNode ret;

  if (!exprNode_isError (pred))
    {
      ret = exprNode_createPartialCopy (pred);
      checkMacroParen (pred);
      exprNode_checkPred (cstring_makeLiteralTemp ("conditional"), pred);
      
      if (!exprNode_isError (ifclause))
	{
	  checkMacroParen (ifclause);   /* update macro counts! */

	  if (!exprNode_isError (elseclause))
	    {
	      checkMacroParen (elseclause);
	      
	      if (!exprNode_matchTypes (ifclause, elseclause))
		{
		  if (gentypeerror 
		      (exprNode_getType (ifclause),
		       ifclause,
		       exprNode_getType (elseclause),
		       elseclause,
		       message ("Conditional clauses are not of same type: "
				"%s (%t), %s (%t)", 
				exprNode_unparse (ifclause), 
				exprNode_getType (ifclause),
				exprNode_unparse (elseclause), 
				exprNode_getType (elseclause)),
		       ifclause->loc))
		    {
		      ret->sref = sRef_undefined;
		      ret->typ = ctype_unknown;
		    }
		}
	      else
		{
		  /* for now...should merge the states */
		  ret->sref = ifclause->sref;
		  ret->typ = ifclause->typ;

		  if (exprNode_isNullValue (ifclause))
		    {
		      ret->typ = elseclause->typ;
		    }
		}
	      
	      exprNode_checkUse (ret, pred->sref, pred->loc);
	      exprNode_checkUse (ifclause, ifclause->sref, ifclause->loc);
	      exprNode_checkUse (elseclause, elseclause->sref, elseclause->loc);

	      exprNode_mergeCondUSs (ret, ifclause, elseclause);

	    }
	  else
	    {
	      ret->typ = ifclause->typ;
	      
	      exprNode_checkUse (pred, pred->sref, pred->loc);
	      exprNode_checkUse (ifclause, ifclause->sref, ifclause->loc);
	      
	      exprNode_mergeCondUSs (ret, ifclause, exprNode_undefined);
	    }
	}
      else 
	{
	  if (!exprNode_isError (elseclause))
	    {
	      ret->typ = elseclause->typ;
	      
	      exprNode_checkUse (pred, pred->sref, pred->loc);
	      exprNode_checkUse (elseclause, elseclause->sref, elseclause->loc);
	      
	      exprNode_mergeCondUSs (ret, exprNode_undefined, elseclause);
	    }
	}
    }
  else /* pred is error */
    {
      if (!exprNode_isError (ifclause))
	{
	  ret = exprNode_createSemiCopy (ifclause);

	  checkMacroParen (ifclause);   /* update macro counts! */
	  
	  if (!exprNode_isError (elseclause))
	    {
	      checkMacroParen (elseclause);
	      
	      ret->typ = ifclause->typ;
	      	      
	      if (!ctype_forceMatch (ifclause->typ, elseclause->typ))
		{
		  if (gentypeerror 
		      (exprNode_getType (ifclause),
		       ifclause,
		       exprNode_getType (elseclause),
		       elseclause,
		       message ("Conditional clauses are not of same type: "
				"%s (%t), %s (%t)", 
				exprNode_unparse (ifclause), 
				exprNode_getType (ifclause),
				exprNode_unparse (elseclause), 
				exprNode_getType (elseclause)),
		       ifclause->loc))
		    {
		      ret->typ = ctype_unknown;
		    }
		}
	      	      
	      exprNode_checkUse (ifclause, ifclause->sref, ifclause->loc);
	      exprNode_checkUse (elseclause, elseclause->sref, elseclause->loc);
	      
	      exprNode_mergeCondUSs (ret, ifclause, elseclause);
	    }
	}
      else if (!exprNode_isError (elseclause)) /* pred, if errors */
	{
	  ret = exprNode_createSemiCopy (ifclause);

	  ret->typ = elseclause->typ;
	  checkMacroParen (elseclause);
	  
	  exprNode_checkUse (elseclause, elseclause->sref, elseclause->loc);
	  exprNode_mergeCondUSs (ret, exprNode_undefined, elseclause);
	}
      else /* all errors! */
	{
	  ret = exprNode_createLoc (ctype_unknown, g_currentloc);
	}
    }
  
  ret->kind = XPR_COND;
  ret->edata = exprData_makeCond (pred, ifclause, elseclause);  

  if (exprNode_isDefined (ifclause) && exprNode_isDefined (elseclause))
    {
      exprNode_combineControl (ret, ifclause, elseclause);
    }

  return (ret);
}

exprNode
exprNode_vaArg (/*@only@*/ lltok tok, /*@only@*/ exprNode arg, /*@only@*/ qtype qt)
{
  ctype totype = qtype_getType (qt);
  exprNode ret =
    exprNode_createPartialLocCopy (arg, fileloc_copy (lltok_getLoc (tok)));
  ctype targ;

  /*
  ** check use of va_arg : <valist>, type -> type
  */

  if (exprNode_isError (arg))
    {
    }
  else
    {
      targ = exprNode_getType (arg);

      /*
      ** arg should have be a pointer
      */

      if (!ctype_isUA (targ) ||
	  (!usymId_equal (ctype_typeId (targ), 
			 usymtab_getTypeId (cstring_makeLiteralTemp ("va_list")))))
	{
	  voptgenerror
	    (FLG_TYPE,
	     message ("First argument to va_arg is not a va_list (type %t): %s",
		      targ, exprNode_unparse (arg)),
	     arg->loc);
	}

      exprNode_checkSet (ret, arg->sref);
    }
  
  /*
  ** return type is totype
  */

  ret->typ = totype;
  ret->kind = XPR_VAARG;
  ret->edata = exprData_makeCast (tok, arg, qt);

  return (ret);
}

exprNode exprNode_labelMarker (/*@only@*/ cstring label)
{
  exprNode ret = exprNode_createPlain (ctype_undefined);
  ret->kind = XPR_LABEL;
  ret->edata = exprData_makeLiteral (label);
  ret->isJumpPoint = TRUE;

  return (ret); /* for now, ignore label */
}

exprNode exprNode_notReached (/*@returned@*/ exprNode stmt)
{
  if (exprNode_isDefined (stmt))
    {
      stmt->isJumpPoint = TRUE;

      /* This prevent stray no return path errors, etc. */
      stmt->exitCode = XK_MUSTEXIT;
    }

  return (stmt); 
}

bool exprNode_isDefaultMarker (exprNode e) 
{
  if (exprNode_isDefined (e))
    {
      return (e->kind == XPR_DEFAULT || e->kind == XPR_FTDEFAULT);
    }

  return FALSE;
}

bool exprNode_isCaseMarker (exprNode e) 
{
  if (exprNode_isDefined (e))
    {
      return (e->kind == XPR_FTCASE || e->kind == XPR_CASE);
    }

  return FALSE;
}

bool exprNode_isLabelMarker (exprNode e) 
{
  if (exprNode_isDefined (e))
    {
      return (e->kind == XPR_LABEL);
    }

  return FALSE;
}

exprNode exprNode_caseMarker (/*@only@*/ exprNode test, bool fallThrough) 
{
  exprNode ret = exprNode_createPartialCopy (test);

  ret->kind = fallThrough ? XPR_FTCASE : XPR_CASE;

  if (exprNode_isError (test)) {
    return ret;
  }

  exprNode_checkUse (ret, test->sref, test->loc);
  
  usymtab_setExitCode (ret->exitCode);
  
  if (ret->mustBreak)
    {
      usymtab_setMustBreak ();
    }

  ret->edata = exprData_makeSingle (test);
  ret->isJumpPoint = TRUE;
  
  return ret;
}

# if 0
exprNode exprNode_caseStatement (/*@only@*/ exprNode test, /*@only@*/ exprNode stmt, bool fallThrough)
{
  exprNode ret = exprNode_createPartialCopy (test);

  ret->kind = fallThrough ? XPR_FTCASE : XPR_CASE;
  ret->edata = exprData_makePair (test, stmt);
  ret->isJumpPoint = TRUE;

  if (exprNode_isError (test))
    {
      return ret;
    }

  exprNode_checkUse (ret, test->sref, test->loc);
  
  if (exprNode_isError (stmt))
    {
      return ret;
    }
  
  exprNode_mergeUSs (ret, stmt);
  
  ret->exitCode = stmt->exitCode;
  ret->mustBreak = stmt->mustBreak;
  ret->canBreak = stmt->canBreak;

  usymtab_setExitCode (ret->exitCode);
  
  if (ret->mustBreak)
    {
      usymtab_setMustBreak ();
    }
  
  return ret;
}
# endif

/*@notnull@*/ /*@only@*/ exprNode 
exprNode_defaultMarker (/*@only@*/ lltok def, bool fallThrough)
{
  exprNode ret = exprNode_createTok (def);
  
  ret->isJumpPoint = TRUE;
  ret->kind = fallThrough ? XPR_FTDEFAULT : XPR_DEFAULT;
  return (ret);
}

bool
exprNode_mayEscape (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return exitkind_couldEscape (e->exitCode);
    }
  return FALSE;
}

static bool
exprNode_mustBreak (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return e->mustBreak;
    }
  return FALSE;
}

bool
exprNode_mustEscape (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return exitkind_mustEscape (e->exitCode) || exprNode_mustBreak (e);
    }

  return FALSE;
}

bool
exprNode_errorEscape (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return exitkind_isError (e->exitCode);
    }

  return FALSE;
}

exprNode exprNode_concat (/*@only@*/ exprNode e1, /*@only@*/ exprNode e2)
{
  exprNode ret = exprNode_createPartialCopy (e1);

  ret->edata = exprData_makePair (e1, e2);
  ret->kind = XPR_STMTLIST;

  if (exprNode_isDefined (e1))
    {
      ret->isJumpPoint = e1->isJumpPoint;
      ret->canBreak = e1->canBreak;
    }
  else
    {
      if (exprNode_isDefined (e2))
	{
	  ret->loc = fileloc_update (ret->loc, e2->loc);
	}
    }

  if (exprNode_isDefined (e2))
    {
      ret->exitCode = e2->exitCode;
      ret->mustBreak = e2->mustBreak;
      if (e2->canBreak) ret->canBreak = TRUE;
    }

  /* 
  ** if e1 must return, then e2 is unreachable!
  */

  if (exprNode_isDefined (e1) && exprNode_isDefined (e2))
    {
      if ((exprNode_mustEscape (e1) || exprNode_mustBreak (e1)) 
	  && !(e2->isJumpPoint))
	{
	  if (context_getFlag (FLG_UNREACHABLE))
	    {
	      exprNode nr = e2;

	      if (e2->kind == XPR_STMT)
		{
		  nr = exprData_getSingle (e2->edata);
		}

	      if ((nr->kind == XPR_TOK 
		   && lltok_isSemi (exprData_getTok (nr->edata))))
		{
		  /* okay to have unreachable ";" */
		  ret->exitCode = XK_MUSTEXIT;
		  ret->canBreak = TRUE;
		}
	      else
		{
		  if (optgenerror (FLG_UNREACHABLE,
				   message ("Unreachable code: %s", 
					    exprNode_unparseFirst (nr)),
				   exprNode_loc (nr)))
		    {
		      ret->isJumpPoint = TRUE;		      
		      ret->mustBreak = FALSE;
		      ret->exitCode = XK_ERROR;
		      DPRINTF (("Jump point: %s", exprNode_unparse (ret)));
		    }
		  else
		    {
		      ret->exitCode = XK_MUSTEXIT;
		      ret->canBreak = TRUE;
		    }

		}
	    }
	}
      else
	{
	  if ((e2->kind == XPR_CASE || e2->kind == XPR_DEFAULT))
	    {
	      /*
              ** We want a warning anytime we have:
	      **         case xxx: ...  
	      **                   yyy;  <<<- no break or return
	      **         case zzz: ...
	      */
	      
	      exprNode lastStmt = exprNode_lastStatement (e1);
	      
	      if (exprNode_isDefined (lastStmt) 
		  && !exprNode_mustEscape (lastStmt)
		  && !exprNode_mustBreak (lastStmt)
		  && !exprNode_isCaseMarker (lastStmt)
		  && !exprNode_isDefaultMarker (lastStmt)
		  && !exprNode_isLabelMarker (lastStmt))
		{
		  voptgenerror (FLG_CASEBREAK,
				cstring_makeLiteral 
				("Fall through case (no preceeding break)"),
				e2->loc);
		}
	    }
	}
    }

  exprNode_mergeUSs (ret, e2);
  
  usymtab_setExitCode (ret->exitCode);
  
  if (ret->mustBreak)
    {
      usymtab_setMustBreak ();
    }

  return ret;
}

exprNode exprNode_createTok (/*@only@*/ lltok t)
{
  exprNode ret = exprNode_create (ctype_unknown);
  ret->kind = XPR_TOK;
  ret->edata = exprData_makeTok (t);
  return ret;
}

exprNode exprNode_statement (/*@only@*/ exprNode e)
{
  if (!exprNode_isError (e))
    {
      exprNode_checkStatement(e);
    }

  return (exprNode_statementError (e));
}

static exprNode exprNode_statementError (/*@only@*/ exprNode e)
{
  exprNode ret = exprNode_createPartialCopy (e);

  if (!exprNode_isError (e))
    {
      if (e->kind != XPR_ASSIGN)
	{
	  exprNode_checkUse (ret, e->sref, e->loc);
	}

      ret->exitCode = e->exitCode;
      ret->canBreak = e->canBreak;
      ret->mustBreak = e->mustBreak;
    }
  
  ret->edata = exprData_makeSingle (e);
  ret->kind = XPR_STMT;

  return ret;
}

exprNode exprNode_checkExpr (/*@returned@*/ exprNode e)
{
  if (!exprNode_isError (e))
    {
      if (e->kind != XPR_ASSIGN)
	{
	  exprNode_checkUse (e, e->sref, e->loc);
	}
    }

  return e;
}

void exprNode_produceGuards (exprNode pred)
{
  if (!exprNode_isError (pred))
    {
      if (ctype_isRealPointer (pred->typ))
	{
	  pred->guards = guardSet_addTrueGuard (pred->guards, pred->sref);
	}
      
      exprNode_checkUse (pred, pred->sref, pred->loc);
      exprNode_resetSref (pred);
    }
}

exprNode exprNode_makeBlock (/*@only@*/ exprNode e)
{
  exprNode ret = exprNode_createPartialCopy (e);

  if (!exprNode_isError (e))
    {
      ret->exitCode = e->exitCode;
      ret->canBreak = e->canBreak;
      ret->mustBreak = e->mustBreak;
    }
  
  ret->edata = exprData_makeSingle (e);
  ret->kind = XPR_BLOCK;
  return ret;
}

bool exprNode_isBlock (exprNode e)
{
  return (exprNode_isDefined (e) 
	  && ((e)->kind == XPR_BLOCK));
}
 
bool exprNode_isAssign (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return (e->kind == XPR_ASSIGN);
    }

  return FALSE;
}

bool exprNode_isEmptyStatement (exprNode e)
{
  return (exprNode_isDefined (e) 
	  && (e->kind == XPR_TOK)
	  && (lltok_isSemi (exprData_getTok (e->edata))));
}

exprNode exprNode_if (/*@only@*/ exprNode pred, /*@only@*/ exprNode tclause)
{
  exprNode ret;
  bool emptyErr = FALSE;

  if (context_maybeSet (FLG_IFEMPTY))
    {
      if (exprNode_isEmptyStatement (tclause))
	{
	  emptyErr = optgenerror (FLG_IFEMPTY,
				  cstring_makeLiteral
				  ("Body of if statement is empty"),
				  exprNode_loc (tclause));
	}
    }

  if (!emptyErr && context_maybeSet (FLG_IFBLOCK))
    {
      if (exprNode_isDefined (tclause)
	  && !exprNode_isBlock (tclause))
	{
	  voptgenerror (FLG_IFBLOCK,
			message
			("Body of if statement is not a block: %s",
			 exprNode_unparse (tclause)),
			exprNode_loc (tclause));
	}
    }

  if (exprNode_isError (pred))
    {
      if (exprNode_isError (tclause))
	{
	  ret = exprNode_createLoc (ctype_unknown, g_currentloc);
	}
      else
	{
	  ret = exprNode_createPartialCopy (tclause);
	}
    }
  else
    {
      if (exprNode_mustEscape (pred))
	{
	  voptgenerror 
	    (FLG_UNREACHABLE,
	     message ("Predicate always exits: %s", exprNode_unparse (pred)),
	     exprNode_loc (pred));
	}

      exprNode_checkPred (cstring_makeLiteralTemp ("if"), pred);
      exprNode_checkUse (pred, pred->sref, pred->loc);
      
      if (!exprNode_isError (tclause))
	{
	  exprNode_mergeCondUSs (pred, tclause, exprNode_undefined);
	}
      
      ret = exprNode_createPartialCopy (pred);
    }

  ret->kind = XPR_IF;
  ret->edata = exprData_makePair (pred, tclause);

  ret->exitCode = XK_UNKNOWN;

  if (exprNode_isDefined (tclause))
    {
      ret->exitCode = exitkind_makeConditional (tclause->exitCode);
      ret->canBreak = tclause->canBreak;
      ret->sets = sRefSet_union (ret->sets, tclause->sets);
      ret->msets = sRefSet_union (ret->msets, tclause->msets);
      ret->uses = sRefSet_union (ret->uses, tclause->uses);
    }

  ret->mustBreak = FALSE;

  return ret;
}

exprNode exprNode_ifelse (/*@only@*/ exprNode pred,
			  /*@only@*/ exprNode tclause, 
			  /*@only@*/ exprNode eclause)
{
  exprNode ret;
  bool tEmptyErr = FALSE;
  bool eEmptyErr = FALSE;

  if (context_maybeSet (FLG_IFEMPTY))
    {
      if (exprNode_isEmptyStatement (tclause))
	{
	  tEmptyErr = optgenerror 
	    (FLG_IFEMPTY,
	     cstring_makeLiteral
	     ("Body of if clause of if statement is empty"),
	     exprNode_loc (tclause));
	}

      if (exprNode_isEmptyStatement (eclause))
	{
	  eEmptyErr = optgenerror 
	    (FLG_IFEMPTY,
	     cstring_makeLiteral
	     ("Body of else clause of if statement is empty"),
	     exprNode_loc (eclause));
	}
    }

  if (context_maybeSet (FLG_IFBLOCK))
    {
      if (!tEmptyErr
	  && exprNode_isDefined (tclause)
	  && !exprNode_isBlock (tclause))
	{
	  voptgenerror (FLG_IFBLOCK,
			message
			("Body of if clause of if statement is not a block: %s",
			 exprNode_unparse (tclause)),
			exprNode_loc (tclause));
	}

      if (!eEmptyErr
	  && exprNode_isDefined (eclause)
	  && !exprNode_isBlock (eclause)
	  && !(eclause->kind == XPR_IF)
	  && !(eclause->kind == XPR_IFELSE))
	{
	  voptgenerror
	    (FLG_IFBLOCK,
	     message
	     ("Body of else clause of if statement is not a block: %s",
	      exprNode_unparse (eclause)),
	     exprNode_loc (eclause));
	}
    }

  if (context_maybeSet (FLG_ELSEIFCOMPLETE))
    {
      if (exprNode_isDefined (eclause)
	  && (eclause->kind == XPR_IF))
	{
	  voptgenerror (FLG_ELSEIFCOMPLETE,
			message ("Incomplete else if logic (no final else): %s",
				 exprNode_unparse (eclause)),
			exprNode_loc (eclause));
	}
    }

  if (exprNode_isError (pred))
    {
      if (exprNode_isError (tclause))
	{
	  if (exprNode_isError (eclause))
	    {
	      ret = exprNode_createLoc (ctype_unknown, g_currentloc);
	    }
	  else
	    {
	      ret = exprNode_createPartialCopy (eclause);
	    }
	}
      else 
	{
	  ret = exprNode_createPartialCopy (tclause);
	}
    }
  else /* pred is okay */
    {
      ret = exprNode_createPartialCopy (pred);

      if (exprNode_mustEscape (pred))
	{
	  voptgenerror
	    (FLG_UNREACHABLE,
	     message ("Predicate always exits: %s", exprNode_unparse (pred)),
	     exprNode_loc (pred));
	}
      
      exprNode_checkPred (cstring_makeLiteralTemp ("if"), pred);
      exprNode_checkUse (ret, pred->sref, pred->loc);
      
      exprNode_mergeCondUSs (ret, tclause, eclause);
    }

  ret->kind = XPR_IFELSE;
  ret->edata = exprData_makeCond (pred, tclause, eclause);

  if (exprNode_isDefined (tclause) && exprNode_isDefined (eclause))
    {
      exprNode_combineControl (ret, tclause, eclause);
      ret->loc = fileloc_update (ret->loc, eclause->loc);
    }

  return ret;
}

/*
** *allpaths <- TRUE iff all executions paths must go through the switch
*/

static bool
checkSwitchExpr (exprNode test, /*@dependent@*/ exprNode e, /*@out@*/ bool *allpaths)
{
  exprNodeSList el = exprNode_flatten (e);
  bool mustReturn = TRUE; /* find a branch that doesn't */
  bool thisReturn = FALSE;
  bool hasDefault = FALSE;
  bool hasAllMembers = FALSE;
  bool inSwitch = FALSE;
  bool isEnumSwitch = FALSE;
  bool canBreak = FALSE;
  bool fallThrough = FALSE;
  ctype ct = ctype_unknown;
  enumNameSList usedEnums;
  enumNameList enums;

  if (exprNode_isDefined (test))
    {
      ctype ttype;

      ct = test->typ;
      ttype = ctype_realType (ct);

      if (ctype_isEnum (ttype))
	{
	  isEnumSwitch = TRUE;
	  enums = ctype_elist (ttype);
	  usedEnums = enumNameSList_new ();
	}
    }

  exprNodeSList_elements (el, current)
    {
      if (exprNode_isDefined (current))
	{
	  switch (current->kind)
	    {
	    case XPR_FTDEFAULT:
	    case XPR_DEFAULT:
	      if (hasDefault)
		{
		  voptgenerror 
		    (FLG_CONTROL,
		     message ("Duplicate default cases in switch"),
		     exprNode_loc (current));
		}          
	    /*@fallthrough@*/
	    case XPR_FTCASE:
	    case XPR_CASE: 
	      if (current->kind == XPR_DEFAULT || current->kind == XPR_FTDEFAULT)
		{
		  hasDefault = TRUE; 
		}
	      else
		{
		  if (isEnumSwitch)
		    {
		      exprNode st = exprData_getSingle (current->edata);
		      uentry ue = exprNode_getUentry (st);
		      
		      if (uentry_isValid (ue))
			{
			  cstring cname = uentry_rawName (ue);
			  
			  if (enumNameList_member (/*@-usedef@*/enums/*@=usedef@*/, cname))
			    {
			      if (enumNameSList_member
				  (/*@-usedef@*/usedEnums/*@=usedef@*/, cname))
				{
				  voptgenerror
				    (FLG_CONTROL,
				     message ("Duplicate case in switch: %s", 
					      cname),
				     current->loc);
				}
			      else
				{
				  enumNameSList_addh (usedEnums, cname);
				}
			    }
			  else
			    {
			      voptgenerror 
				(FLG_TYPE,
				 message ("Case in switch not %s member: %s", 
					  ctype_unparse (ct), cname),
				 current->loc);
			    }
			}
		    }
		}
	      
	      if (inSwitch && !fallThrough)
		{
		  if (!thisReturn || canBreak) 
		    {
		      mustReturn = FALSE;
		    }
		}
	      
	      fallThrough = TRUE;
	      inSwitch = TRUE;
	      thisReturn = FALSE;
	      canBreak = FALSE;
	      /*@switchbreak@*/ break;
	    default:
	      thisReturn = thisReturn || exprNode_mustEscape (current);
	      canBreak = canBreak || current->canBreak;
	      if (canBreak) fallThrough = FALSE;
	    }
	}
    } end_exprNodeSList_elements;

  if (inSwitch) /* check the last one! */
    {
      if (!thisReturn || canBreak) 
	{
	  mustReturn = FALSE;
	}
    }
  
  if (isEnumSwitch)
    {
      if (!hasDefault 
	  && (enumNameSList_size (/*@-usedef@*/usedEnums/*@=usedef@*/) != 
	      enumNameList_size (/*@-usedef@*/enums/*@=usedef@*/)))
	{
	  enumNameSList unused = enumNameSList_subtract (enums, usedEnums);
	  
	  voptgenerror (FLG_MISSCASE,
			message ("Missing case%s in switch: %q",
				 cstring_makeLiteralTemp 
				 ((enumNameSList_size (unused) > 1) ? "s" : ""),
				 enumNameSList_unparse (unused)),
			g_currentloc);

	  enumNameSList_free (unused);
	}
      else
	{
	  hasAllMembers = TRUE;
	  *allpaths = TRUE;
	}

      enumNameSList_free (usedEnums);
    }
  else
    {
      *allpaths = hasDefault;
    }

  exprNodeSList_free (el);
  return ((hasDefault || hasAllMembers) && mustReturn);  
}
    
exprNode exprNode_switch (/*@only@*/ exprNode e, /*@only@*/ exprNode s)
{
  exprNode ret = exprNode_createPartialCopy (e);
  bool allpaths;

  DPRINTF (("Switch: %s", exprNode_unparse (s)));

  ret->kind = XPR_SWITCH;
  ret->edata = exprData_makePair (e, s);
  
  if (!exprNode_isError (s))
    {
      exprNode fs = exprNode_firstStatement (s);
      ret->loc = fileloc_update (ret->loc, s->loc);

      if (exprNode_isUndefined (fs) 
	  || exprNode_isCaseMarker (fs) || exprNode_isLabelMarker (fs)
	  || exprNode_isDefaultMarker (fs)) {
	;
      } else {
	voptgenerror (FLG_FIRSTCASE,
		      message
		      ("Statement after switch is not a case: %s", exprNode_unparse (fs)),
		      fs->loc);
      }
    }

  if (!exprNode_isError (e)) 
    {
      if (checkSwitchExpr (e, s, &allpaths))
	{
	  ret->exitCode = XK_MUSTRETURN;
	}
      else
	{
	  ret->exitCode = e->exitCode;
	}

      ret->canBreak = e->canBreak;
      ret->mustBreak = e->mustBreak;
    }
  /*
  ** forgot this!
  **   exprNode.c:3883,32: Variable allpaths used before definition
  */
  else 
    {
      allpaths = FALSE;
    }
  
  DPRINTF (("Context exit switch!"));
  context_exitSwitch (ret, allpaths);
  DPRINTF (("Context exit switch done!"));

  return ret;
}

static void checkInfiniteLoop (/*@notnull@*/ exprNode test,
			       /*@notnull@*/ exprNode body)
{
  sRefSet tuses = test->uses;
  
  if (!sRefSet_isEmpty (test->uses))
    {
      sRefSet sets = sRefSet_newCopy (body->sets);
      bool hasError = TRUE;
      bool innerState = FALSE;
      sRefSet tuncon = sRefSet_undefined;
      
      sets = sRefSet_union (sets, test->sets);
      sets = sRefSet_union (sets, body->msets);
      sets = sRefSet_union (sets, test->msets);

      sRefSet_allElements (tuses, el)
	{
	  if (sRef_isUnconstrained (el))
	    {
	      tuncon = sRefSet_insert (tuncon, el);
	    }
	  else
	    {
	      if (sRefSet_member (sets, el))
		{
		  hasError = FALSE;
		  break;
		}
	    }

	  if (sRef_isInternalState (el)
	      || sRef_isFileStatic (sRef_getRootBase (el)))
	    {
	      innerState = TRUE;
	    }
	} end_sRefSet_allElements ;

      if (hasError)
	{
	  sRefSet suncon = sRefSet_undefined;
	  bool sinner = FALSE;

	  sRefSet_allElements (sets, el)
	    {
	      if (sRef_isUnconstrained (el))
		{
		  suncon = sRefSet_insert (suncon, el);
		}
	      else if (sRef_isInternalState (el))
		{
		  sinner = TRUE;
		}
	      else
		{
		  ;
		}
	    } end_sRefSet_allElements ;

	  if (sinner && innerState)
	    {
	      ; 
	    }
	  else if (sRefSet_isEmpty (tuncon)
		   && sRefSet_isEmpty (suncon))
	    {
	      voptgenerror 
		(FLG_INFLOOPS,
		 message
		 ("Suspected infinite loop.  No value used in loop test (%q) "
		  "is modified by test or loop body.",
		  sRefSet_unparsePlain (tuses)), 
		 test->loc);
	    }
	  else
	    {
	      if (sRefSet_isEmpty (tuncon))
		{
		  voptgenerror 
		    (FLG_INFLOOPSUNCON,
		     message ("Suspected infinite loop.  No condition values "
			      "modified.  Modification possible through "
			      "unconstrained calls: %q",
			      sRefSet_unparsePlain (suncon)), 
		     test->loc);
		}
	      else
		{
		  voptgenerror 
		    (FLG_INFLOOPSUNCON,
		     message ("Suspected infinite loop.  No condition values "
			      "modified.  Possible undetected dependency through "
			      "unconstrained calls in loop test: %q",
			      sRefSet_unparsePlain (tuncon)), 
		     test->loc);
		}
	    }
	}

      sRefSet_free (sets);
    }
}

exprNode exprNode_while (/*@keep@*/ exprNode t, /*@keep@*/ exprNode b)
{
  exprNode ret;
  bool emptyErr = FALSE;
  
  if (context_maybeSet (FLG_WHILEEMPTY))
    {
      if (exprNode_isEmptyStatement (b))
	{
	  emptyErr = optgenerror 
	    (FLG_WHILEEMPTY,
	     cstring_makeLiteral
	     ("Body of while statement is empty"),
	     exprNode_loc (b));
	}
    }

  if (!emptyErr && context_maybeSet (FLG_WHILEBLOCK))
    {
      if (exprNode_isDefined (b)
	  && !exprNode_isBlock (b))
	{
	  if (context_inIterDef ()
	      && (b->kind == XPR_STMTLIST
		  || b->kind == XPR_TOK))
	    {
	      ; /* no error */
	    }
	  else
	    {
	      voptgenerror (FLG_WHILEBLOCK,
			    message
			    ("Body of while statement is not a block: %s",
			     exprNode_unparse (b)),
			    exprNode_loc (b));
	    }
	}
    }
  
  if (exprNode_isError (t))
    {
      if (exprNode_isError (b))
	{
	  ret = exprNode_createLoc (ctype_unknown, g_currentloc);
	}
      else
	{
	  ret = exprNode_createPartialCopy (b);
	}
    }
  else
    {
      exprNode test;

      ret = exprNode_createPartialCopy (t);
      
      llassert (t->kind == XPR_WHILEPRED);

      test = exprData_getSingle (t->edata);

      if (!exprNode_isError (b) && exprNode_isDefined (test))
	{
	  if (context_maybeSet (FLG_INFLOOPS)
	      || context_maybeSet (FLG_INFLOOPSUNCON))
	    {
	      /*
	      ** check that some variable in the predicate is set by the body
              ** if the predicate uses any variables
              */
	      
	      checkInfiniteLoop (test, b);
	    }

	  exprNode_mergeUSs (ret, b);

	  if (exprNode_isDefined (b))
	    {
	      ret->exitCode = exitkind_makeConditional (b->exitCode);
	    }
	}
    }
  
  ret->edata = exprData_makePair (t, b);
  ret->kind = XPR_WHILE;

  if (exprNode_isDefined (t) && exprNode_mustEscape (t))
    {
      voptgenerror
	(FLG_CONTROL,
	 message ("Predicate always exits: %s", exprNode_unparse (t)),
	 exprNode_loc (t));
    }

  ret->exitCode = XK_NEVERESCAPE;

  /*
  ** If loop is infinite, and there is no break inside, 
  ** exit code is never reach. 
  */

  if (exprNode_knownIntValue (t))
    {
      if (!exprNode_isZero (t)) 
	{
	  if (exprNode_isDefined (b)) 
	    {
	      if (!b->canBreak) 
		{
		  /* Really, it means never reached. */
		  ret->exitCode = XK_MUSTEXIT;
		}
	    }
	}
    } 
  else 
    {
      ;
    }

  ret->canBreak = FALSE;
  ret->mustBreak = FALSE;

  return ret; 
}

/*
** do { b } while (t);
** 
** note: body passed as first argument 
*/

exprNode exprNode_doWhile (/*@only@*/ exprNode b, /*@only@*/ exprNode t)
{
  exprNode ret;
  
  if (exprNode_isError (t))
    {
      if (exprNode_isError (b))
	{
	  ret = exprNode_createLoc (ctype_unknown, g_currentloc);
	}
      else
	{
	  ret = exprNode_createPartialCopy (b);

	  ret->exitCode = exitkind_makeConditional (b->exitCode);
	  exprNode_checkUse (ret, b->sref, b->loc);
	  ret->exitCode = b->exitCode;
	  ret->canBreak = b->canBreak;
	  ret->mustBreak = b->mustBreak;
	}
    }
  else
    {
      ret = exprNode_createPartialCopy (t);
      exprNode_checkPred (cstring_makeLiteralTemp ("while"), t);
      
      if (!exprNode_isError (b)) 
	{
	  /*
	  ** forgot the copy's --- why wasn't this detected??
	  */

	  ret->sets = sRefSet_copy (ret->sets, b->sets);
	  ret->msets = sRefSet_copy (ret->msets, b->msets);
	  ret->uses = sRefSet_copy (ret->uses, b->uses);  

	  /* left this out --- causes and aliasing bug (infinite loop)
	     should be detected?? */

	  exprNode_checkUse (ret, b->sref, b->loc);
	  exprNode_mergeUSs (ret, t);
	  exprNode_checkUse (ret, t->sref, t->loc);

	  ret->exitCode = b->exitCode;
	  ret->canBreak = b->canBreak;
	  ret->mustBreak = b->mustBreak;
	}
    }
  
  context_exitDoWhileClause (t);

  ret->kind = XPR_DOWHILE;
  ret->edata = exprData_makePair (t, b);
    return ret;
}

exprNode exprNode_for (/*@keep@*/ exprNode inc, /*@keep@*/ exprNode body)
{
  exprNode ret;
  bool emptyErr = FALSE;

  if (context_maybeSet (FLG_FOREMPTY))
    {
      if (exprNode_isEmptyStatement (body))
	{
	  emptyErr = optgenerror 
	    (FLG_FOREMPTY,
	     cstring_makeLiteral
	     ("Body of for statement is empty"),
	     exprNode_loc (body));
	}
    }

  if (!emptyErr && context_maybeSet (FLG_FORBLOCK))
    {
      if (exprNode_isDefined (body)
	  && !exprNode_isBlock (body))
	{
	  if (context_inIterDef ()
	      && (body->kind == XPR_STMTLIST
		  || body->kind == XPR_TOK))
	    {
	      ; /* no error */
	    }
	  else
	    {
	      voptgenerror (FLG_FORBLOCK,
			    message
			    ("Body of for statement is not a block: %s",
			     exprNode_unparse (body)),
			    exprNode_loc (body));
	    }
	}
    }

  /*
  ** for ud purposes:  (alreadly) init -> test -> (now) LOOP: body + inc + test
  */

  if (exprNode_isError (body))
    {
      ret = exprNode_createPartialCopy (inc);
    }
  else
    {
      ret = exprNode_createPartialCopy (body);
      
      ret->exitCode = exitkind_makeConditional (body->exitCode);

            exprNode_mergeUSs (inc, body);
      
      if (exprNode_isDefined (inc))
	{
	  exprNode tmp;

	  context_setMessageAnnote (cstring_makeLiteral ("in post loop increment"));
     
	  
	  tmp = exprNode_effect (exprData_getTripleInc (inc->edata));
	  exprNode_freeShallow (tmp); 

	  context_clearMessageAnnote ();
	  context_setMessageAnnote (cstring_makeLiteral ("in post loop test"));

	  tmp = exprNode_effect (exprData_getTripleTest (inc->edata));
	  exprNode_freeShallow (tmp);

	  context_clearMessageAnnote ();

	  ret->uses = sRefSet_copy (ret->uses, inc->uses);
	  ret->sets = sRefSet_copy (ret->sets, inc->sets);
	  ret->msets = sRefSet_copy (ret->msets, inc->msets);
	}
    }

  ret->kind = XPR_FOR;
  ret->edata = exprData_makePair (inc, body);

  if (exprNode_isDefined (inc)) {
    exprNode test = exprData_getTripleTest (inc->edata);

    if (exprNode_isUndefined (test)) {
      if (exprNode_isDefined (body)) {
	if (!body->canBreak) {
	  /* Really, it means never reached. */
	  ret->exitCode = XK_MUSTEXIT;
	}
      }
    }
  }

  return (ret);
}

/*
** for (init; test; inc)
** ==>
** init;
** while (test) { body; inc; } 
**
** Now: check use of init (may set vars for test)
**      check use of test
**      no checks on inc
_*/

/*@observer@*/ guardSet exprNode_getForGuards (exprNode pred)
{
  exprNode test;

  if (exprNode_isError (pred)) return guardSet_undefined;

  llassert (pred->kind == XPR_FORPRED);

  test = exprData_getTripleTest (pred->edata);

  if (!exprNode_isError (test))
    {
      return (test->guards);
    }

  return guardSet_undefined;
}

exprNode exprNode_whilePred (/*@only@*/ exprNode test)
{
  exprNode ret = exprNode_createSemiCopy (test);

  if (exprNode_isDefined (test))
    {
      exprNode_copySets (ret, test);
      exprNode_checkPred (cstring_makeLiteralTemp ("while"), test);
      exprNode_checkUse (ret, test->sref, test->loc);
      
      exprNode_produceGuards (test);
      
      ret->guards = guardSet_copy (test->guards);
    }

  ret->edata = exprData_makeSingle (test);
  ret->kind = XPR_WHILEPRED;
  return ret;
}

exprNode exprNode_forPred (/*@only@*/ exprNode init, /*@only@*/ exprNode test, 
			   /*@only@*/ exprNode inc)
{
  exprNode ret;
  
  /*
  ** for ud purposes:  init -> test -> LOOP: [ body, inc ]
  */
  
  exprNode_checkPred (cstring_makeLiteralTemp ("for"), test);

  if (!exprNode_isError (inc)) 
    {
      ret = exprNode_createPartialCopy (inc);
    }
  else 
    {
      if (!exprNode_isError (init)) 
	{
	  ret = exprNode_createPartialCopy (init);
	}
      else if (!exprNode_isError (test)) 
	{
	  ret = exprNode_createPartialCopy (test);
	}
      else 
	{
	  ret = exprNode_createUnknown ();
	}
    }

  exprNode_mergeUSs (ret, init);

  if (exprNode_isDefined (init))
    {
      exprNode_checkUse (ret, init->sref, init->loc);
    }

  exprNode_mergeUSs (ret, test);

  if (exprNode_isDefined (test))
    {
      exprNode_checkUse (ret, test->sref, test->loc);
    }

  ret->kind = XPR_FORPRED;
  ret->edata = exprData_makeFor (init, test, inc); 
  return (ret);
}

/*@notnull@*/ /*@only@*/ exprNode exprNode_goto (/*@only@*/ cstring label)
{
  exprNode ret = exprNode_createUnknown ();

  if (context_inMacro ())
    {
      voptgenerror (FLG_MACROSTMT,
		    message ("Macro %s uses goto (not functional)", 
			     context_inFunctionName ()),
		    g_currentloc);
    }
  
  ret->kind = XPR_GOTO;
  ret->edata = exprData_makeLiteral (label);
  ret->mustBreak = TRUE;
  ret->exitCode = XK_GOTO;
  ret->canBreak = TRUE;
  return ret;
}

exprNode exprNode_continue (/*@only@*/ lltok l, int qcontinue)
{
  exprNode ret = exprNode_createLoc (ctype_unknown, fileloc_copy (lltok_getLoc (l)));

  ret->kind = XPR_CONTINUE;
  ret->edata = exprData_makeTok (l);
  ret->canBreak = TRUE;
  ret->mustBreak = TRUE;

  if (qcontinue == QSAFEBREAK)
    {
      ; /* no checking */
    }
  else if (qcontinue == QINNERCONTINUE)
    {
      if (!context_inDeepLoop ())
	{
	  voptgenerror 
	    (FLG_LOOPLOOPCONTINUE,
	     cstring_makeLiteral ("Continue statement marked with innercontinue "
				  "is not inside a nested loop"),
	     exprNode_loc (ret));
	}
    }
  else if (qcontinue == BADTOK)
    {
      if (context_inDeepLoop ())
	{
	  voptgenerror 
	    (FLG_LOOPLOOPCONTINUE,
	     cstring_makeLiteral ("Continue statement in nested loop"),
	     exprNode_loc (ret));
	}
    }
  else
    {
      llbuglit ("exprNode_continue: bad qcontinue");
    }

  return ret;
}

exprNode exprNode_break (/*@only@*/ lltok l, int bqual)
{
  exprNode ret = exprNode_createLoc (ctype_unknown, fileloc_copy (lltok_getLoc (l)));
  clause breakClause = context_breakClause ();
  
  ret->kind = XPR_BREAK;
  ret->edata = exprData_makeTok (l);
  ret->canBreak = TRUE;
  ret->mustBreak = TRUE;
  
  if (breakClause == NOCLAUSE)
    {
      voptgenerror 
	(FLG_SYNTAX,
	 cstring_makeLiteral ("Break not inside while, for or switch statement"),
	 exprNode_loc (ret));
    }
  else
    {
      if (bqual != BADTOK)
	{
	  switch (bqual)
	    {
	    case QSAFEBREAK:
	      break;
	    case QINNERBREAK:
	      if (breakClause == SWITCHCLAUSE)
		{
		  if (!context_inDeepSwitch ())
		    {
		      voptgenerror (FLG_SYNTAX,
				    cstring_makeLiteral 
				    ("Break preceded by innerbreak is not in a deep switch"),
				    exprNode_loc (ret));
		    }
		}
	      else 
		{
		  if (!context_inDeepLoop ())
		    {
		      voptgenerror (FLG_SYNTAX,
				    cstring_makeLiteral 
				    ("Break preceded by innerbreak is not in a deep loop"),
				    exprNode_loc (ret));
		    }
		}
	      break;
	    case QLOOPBREAK:
	      if (breakClause == SWITCHCLAUSE)
		{
		  voptgenerror (FLG_SYNTAX,
				cstring_makeLiteral 
				("Break preceded by loopbreak is breaking a switch"),
				exprNode_loc (ret));
		}
	      break;
	    case QSWITCHBREAK:
	      if (breakClause != SWITCHCLAUSE)
		{
		  voptgenerror 
		    (FLG_SYNTAX,
		     message ("Break preceded by switchbreak is breaking %s",
			      cstring_makeLiteralTemp 
			      ((breakClause == WHILECLAUSE
				|| breakClause == DOWHILECLAUSE) ? "a while loop"
			       : (breakClause == FORCLAUSE) ? "a for loop"
			       : (breakClause == ITERCLAUSE) ? "an iterator"
			       : "<error loop>")),
		     exprNode_loc (ret));
		}
	      break;
	    BADDEFAULT;
	    }
	}
      else
	{
	  if (breakClause == SWITCHCLAUSE)
	    {
	      clause nextBreakClause = context_nextBreakClause ();

	      switch (nextBreakClause)
		{
		case NOCLAUSE: break;
		case WHILECLAUSE:
		case DOWHILECLAUSE:
		case FORCLAUSE:
		case ITERCLAUSE:
		  voptgenerror 
		    (FLG_LOOPSWITCHBREAK,
		     cstring_makeLiteral ("Break statement in switch inside loop"),
		     exprNode_loc (ret));
		  break;
		case SWITCHCLAUSE:
		  voptgenerror 
		    (FLG_SWITCHSWITCHBREAK,
		     cstring_makeLiteral ("Break statement in switch inside switch"),
		     exprNode_loc (ret));
		  break;
		BADDEFAULT;
		}
	    }
	  else
	    {
	      if (context_inDeepLoop ())
		{
		  voptgenerror 
		    (FLG_LOOPLOOPBREAK,
		     cstring_makeLiteral ("Break statement in nested loop"),
		     exprNode_loc (ret));
		}
	      else 
		{
		  if (context_inDeepLoopSwitch ())
		    {
		      voptgenerror 
			(FLG_SWITCHLOOPBREAK,
			 cstring_makeLiteral ("Break statement in loop inside switch"),
			 exprNode_loc (ret));
		    }
		}
	    }
	}
    }

  return ret;
}

exprNode exprNode_nullReturn (/*@only@*/ lltok t)
{
  fileloc loc = lltok_getLoc (t);
  exprNode ret = exprNode_createLoc (ctype_unknown, fileloc_copy (loc));
  
  context_returnFunction ();
  exprChecks_checkNullReturn (loc);

  ret->kind = XPR_NULLRETURN;
  ret->edata = exprData_makeTok (t);
  ret->exitCode = XK_MUSTRETURN;
  return ret;
}

exprNode exprNode_return (/*@only@*/ exprNode e)
{
  exprNode ret;
  
  if (exprNode_isError (e))
    {
      ret = exprNode_createUnknown ();
    }
  else
    {
      ret = exprNode_createLoc (ctype_unknown, fileloc_copy (e->loc));

      exprNode_checkUse (ret, e->sref, e->loc);
      exprNode_checkReturn (e);
    }

  context_returnFunction ();
  ret->kind = XPR_RETURN;
  ret->edata = exprData_makeSingle (e);
  ret->exitCode = XK_MUSTRETURN;

  return (ret);
}

exprNode exprNode_comma (/*@only@*/ exprNode e1, /*@only@*/ exprNode e2)
{
  exprNode ret;

  if (exprNode_isError (e1)) 
    {
      if (exprNode_isError (e2))
	{
	  ret = exprNode_createLoc (ctype_unknown, g_currentloc);
	}
      else
	{
	  ret = exprNode_createPartialCopy (e2);
	  exprNode_checkUse (ret, e2->sref, e2->loc);
	  ret->sref = e2->sref;
	}
    }
  else
    {
      ret = exprNode_createPartialCopy (e1);

      exprNode_checkUse (ret, e1->sref, e1->loc);

      if (!exprNode_isError (e2))
	{
	  exprNode_mergeUSs (ret, e2);
	  exprNode_checkUse (ret, e2->sref, e2->loc);
	  ret->sref = e2->sref;
	}
    }

  ret->kind = XPR_COMMA;
  ret->edata = exprData_makePair (e1, e2);
  
  if (exprNode_isDefined (e1))
    {
      if (exprNode_isDefined (e2))
	{
	  ret->typ = e2->typ; 

	  if (exprNode_mustEscape (e1) || e1->mustBreak)
	    {
	      voptgenerror 
		(FLG_UNREACHABLE,
		 message ("Second clause of comma expression is unreachable: %s",
			  exprNode_unparse (e2)), 
		 exprNode_loc (e2));
	    }
	  
	  ret->exitCode = exitkind_combine (e1->exitCode, e2->exitCode);
	  ret->mustBreak = e1->mustBreak || e2->mustBreak;
	  ret->canBreak = e1->canBreak || e2->canBreak;
	}
      else
	{
	  if (exprNode_mustEscape (e1) || e1->mustBreak)
	    {
	      voptgenerror 
		(FLG_UNREACHABLE,
		 message ("Second clause of comma expression is unreachable: %s",
			  exprNode_unparse (e2)), 
		 exprNode_loc (e2));
	    }
	  
	  ret->exitCode = e1->exitCode;
	  ret->canBreak = e1->canBreak;
	}
    }
  else
    {
      if (exprNode_isDefined (e2))
	{
	  ret->exitCode = e2->exitCode;
	  ret->mustBreak = e2->mustBreak;
	  ret->canBreak = e2->canBreak;
	}
    }

  return (ret);
}

static bool exprNode_checkOneInit (/*@notnull@*/ exprNode el, exprNode val)
{
  ctype t1 = exprNode_getType (el);
  ctype t2 = exprNode_getType (val);
  bool hasError = FALSE;
  
  if (ctype_isUnknown (t1))
    {
      voptgenerror (FLG_IMPTYPE,
		    message ("Variable has unknown (implicitly int) type: %s",
			     exprNode_unparse (el)),
		    el->loc);
      
      t1 = ctype_int;
      el->typ = ctype_int;
    }

  if (exprNode_isDefined (val) && val->kind == XPR_INITBLOCK)
    {
      exprNodeList vals = exprData_getArgs (val->edata);

      if (ctype_isRealAP (t1))
	{
	  int i = 0;
	  int nerrors = 0;

	  exprNodeList_elements (vals, oneval)
	    {
	      cstring istring = message ("%d", i);
	      exprNode newel =
		exprNode_arrayFetch 
		  (exprNode_fakeCopy (el),
		   exprNode_numLiteral (ctype_int, istring,
					fileloc_copy (el->loc), i));
	      
	      if (exprNode_isDefined (newel))
		{
		  if (exprNodeList_size (vals) == 1
		      && ctype_isString (exprNode_getType (oneval))
		      && ctype_isChar (exprNode_getType (newel)))
		    {
		      exprNode_freeIniter (newel);
		    }
		  else
		    {
		      if (exprNode_checkOneInit (newel, oneval))
			{
			  hasError = TRUE;
			  nerrors++;
			  
			  if (nerrors > 3 && exprNodeList_size (vals) > 6)
			    {
			      llgenmsg 
				(message ("Additional initialization errors "
					  "for %s not reported",
					  exprNode_unparse (el)),
				 exprNode_loc (el));
			      exprNode_freeIniter (newel);
			      break;
			    }
			  else
			    {
			      exprNode_freeIniter (newel);
			    }
			}
		      else
			{
			  exprNode_freeIniter (newel);
			}
		    }
		}

	      cstring_free (istring);
	      i++;
	      /*@-branchstate@*/ 
	    } end_exprNodeList_elements;
	  /*@=branchstate@*/
	}
      else if (ctype_isStruct (ctype_realType (t1)))
	{
	  uentryList fields = ctype_getFields (t1);
	  int i = 0;

	  if (uentryList_size (fields) != exprNodeList_size (vals))
	    {
	      if (uentryList_size (fields) > exprNodeList_size (vals))
		{
		  hasError = optgenerror 
		    (FLG_FULLINITBLOCK,
		     message ("Initializer block for "
			      "%s has %d field%p, but %s has %d field%p: %q",
			      exprNode_unparse (el),
			      exprNodeList_size (vals),
			      ctype_unparse (t1),
			      uentryList_size (fields),
			      exprNodeList_unparse (vals)),
		     val->loc);	  
		}
	      else
		{
		  hasError = optgenerror 
		    (FLG_TYPE,
		     message ("Initializer block for "
			      "%s has %d field%p, but %s has %d field%p: %q",
			      exprNode_unparse (el),
			      exprNodeList_size (vals),
			      ctype_unparse (t1),
			      uentryList_size (fields),
			      exprNodeList_unparse (vals)),
		     val->loc);	  
		}
	    }
	  else
	    {
	      exprNodeList_elements (vals, oneval)
		{
		  uentry thisfield = uentryList_getN (fields, i);
		  exprNode newel =
		    exprNode_fieldAccess (exprNode_fakeCopy (el),
					  uentry_getName (thisfield));

		  if (exprNode_isDefined (newel))
		    {
		      if (exprNode_checkOneInit (newel, oneval))
			{
			  hasError = TRUE;
			}

		      exprNode_freeIniter (newel);
		    }

		  i++;
		} end_exprNodeList_elements;
	    }
	}
      else
	{
	  hasError = optgenerror 
	    (FLG_TYPE,
	     message ("Initializer block used for "
		      "%s where %t is expected: %s",
		      exprNode_unparse (el), t1, exprNode_unparse (val)),
	     val->loc);	  
	}
    }
  else
    {
      if (exprNode_isDefined (val))
	{
	  doAssign (el, val, TRUE);
	  
	  if (!exprNode_matchType (t1, val))
	    {
	      hasError = gentypeerror 
		(t1, val, t2, el,
		 message ("Initial value of %s is type %t, "
			  "expects %t: %s",
			  exprNode_unparse (el),
			  t2, t1, exprNode_unparse (val)),
		 val->loc);
	    }
	}
    }

  return hasError;
}

exprNode exprNode_makeInitialization (/*@only@*/ idDecl t,
				      /*@only@*/ exprNode e)
{
  uentry ue = usymtab_lookup (idDecl_observeId (t));
  bool isUsed = uentry_isUsed (ue);
  exprNode ret = exprNode_fromIdentifierAux (ue);
  ctype ct = ctype_realishType (ret->typ);
  fileloc loc;
  
  if (ctype_isUnknown (ct)) 
    {
      voptgenerror (FLG_IMPTYPE,
		    message ("Variable has unknown (implicitly int) type: %s",
			     idDecl_getName (t)),
		    exprNode_isDefined (e) ? exprNode_loc (e) : g_currentloc);
      
      ct = ctype_int;
    }

  if (exprNode_isError (e)) 
    {
      e = exprNode_createUnknown ();
      loc = g_currentloc;

      /* error: assume initializer is defined */
      sRef_setDefined (ret->sref, loc);
    }
  else
    {
      loc = exprNode_loc (e);
      
      /*
      ** evs - 9 Apr 1995
      **
      ** was addSafeUse --- what's the problem?
      **
      **   int x = 3, y = x ?
      */

      exprNode_checkUse (ret, e->sref, e->loc);
      
      if (ctype_isUnknown (e->typ) && uentry_isValid (ue))
	{
	  exprNode lhs = exprNode_createId (ue);

	  /*
	  ** static storage should be undefined before initializing
	  */

	  if (uentry_isStatic (ue))
	    {
	      sRef_setDefState (lhs->sref, SS_PARTIAL, fileloc_undefined);
	    }

	  (void) exprNode_checkOneInit (lhs, e);

	  if (uentry_isStatic (ue))
	    {
	      sRef_setDefState (lhs->sref, SS_DEFINED, fileloc_undefined);
	    }

	  exprNode_free (lhs);
	}
      else
	{
	  if (!exprNode_matchType (ct, e))
	    {
	      if (exprNode_isZero (e) && ctype_isArrayPtr (ct)) 
		{
		  ;
		}
	      else
		{
		  (void) gentypeerror 
		    (exprNode_getType (e), e, exprNode_getType (ret), ret,
		     message 
		     ("Variable %s initialized to type %t, expects %t: %s",
		      exprNode_unparse (ret), exprNode_getType (e), 
		      exprNode_getType (ret),
		      exprNode_unparse (e)),
		     e->loc);
		}
	    }
	}
      
      if (uentry_isStatic (ue))
	{
	  sRef_setDefState (ret->sref, SS_PARTIAL, fileloc_undefined);
	}

      doAssign (ret, e, TRUE);

      if (uentry_isStatic (ue))
	{
	  sRef_setDefState (ret->sref, SS_DEFINED, fileloc_undefined);
	}
    }

  if (context_inIterDef ())
    {
      /* should check if it is yield */
      uentry_setUsed (ue, loc);
    }
  else
    {
      if (!isUsed) /* could be @unused@-qualified variable */
	{
	  uentry_setNotUsed (ue);  
	}
    }

  ret->exitCode = XK_NEVERESCAPE;
  ret->mustBreak = FALSE;

  /*
  ** Must be before new kind is assigned!
  */

  exprData_free (ret->edata, ret->kind); 

  ret->kind = XPR_INIT;
  ret->edata = exprData_makeInit (t, e);
  exprNode_mergeUSs (ret, e);
  return ret;
}
  
exprNode exprNode_iter (/*@observer@*/ uentry name,
			/*@only@*/ exprNodeList alist, 
			/*@only@*/ exprNode body,
			/*@observer@*/ uentry end)
{
  exprNode ret;
  cstring iname;

  llassert (uentry_isValid (name));

  uentry_setUsed (name, exprNode_loc (body));

  ret = exprNode_createPartialCopy (body);
  iname = uentry_getName (name);

  if (uentry_isInvalid (end))
    {
      llerror (FLG_ITER,
	       message ("Iter %s not balanced with end_%s", iname, iname));
    }
  else
    {
      cstring ename = uentry_getName (end);

      if (!cstring_equalPrefix (ename, "end_"))
	{
	  llerror (FLG_ITER, message ("Iter %s not balanced with end_%s: %s", 
				      iname, iname, ename));
	}
      else
	{
	  if (!cstring_equal (iname, cstring_suffix (ename, 4)))
	    {
	      llerror (FLG_ITER, 
		       message ("Iter %s not balanced with end_%s: %s", 
				iname, iname, ename));
	    }
	}

      cstring_free (ename);
    }

  context_exitIterClause (body);
  
  ret->kind = XPR_ITER;
  ret->edata = exprData_makeIter (name, alist, body, end);

  if (uentry_isIter (name))
    {
      (void) checkArgsReal (name, body, 
			    uentry_getParams (name), alist, TRUE, ret);
    }

  cstring_free (iname);

  return ret;
}

exprNode
exprNode_iterNewId (/*@only@*/ cstring s)
{
  exprNode e = exprNode_new ();
  uentry ue = uentryList_getN (uentry_getParams (getCurrentIter ()), iterParamNo ());

  llassert (processingIterVars ());

  e->loc = context_getSaveLocation ();

  if (fileloc_isUndefined (e->loc))
    {
      fileloc_free (e->loc);
      e->loc = fileloc_copy (g_currentloc);
    }

  e->uses = sRefSet_new ();
  e->sets = sRefSet_new ();
  e->msets = sRefSet_new ();
  e->kind = XPR_VAR;
  e->val = multiVal_unknown ();
  e->guards = guardSet_new ();
  e->sref = defref;
  e->isJumpPoint = FALSE;
  e->exitCode = XK_NEVERESCAPE;

  /*> missing fields, detected by lclint <*/
  e->canBreak = FALSE;
  e->mustBreak = FALSE;
  e->etext = cstring_undefined;

  if (uentry_isYield (ue))
    {
      uentry uue = uentry_makeVariable (s, uentry_getType (ue), 
					fileloc_copy (e->loc), 
					FALSE);
      sRef sr;

      uue = usymtab_supEntrySrefReturn (uue);

      sr = uentry_getSref (uue);
      sRef_mergeStateQuiet (sr, uentry_getSref (ue));
      sr = uentry_getSref (uue);
      sRef_setDefined (sr, e->loc);

      e->typ = uentry_getType (uue);      
      e->sref = sr;
      e->edata = exprData_makeId (uue);
      uentry_setUsed (uue, g_currentloc);
    }
  else
    {
      uentry uue;

      sRef_setGlobalScope ();
      uue = uentry_makeVariableLoc (s, ctype_unknown);

      e->typ = ctype_unknown;
      e->edata = exprData_makeId (uue);

      uentry_setUsed (uue, e->loc);
      uentry_setHasNameError (uue); 

      if (context_getFlag (FLG_REPEATUNRECOG))
	{
	  uentry_markOwned (uue);
	}
      else
	{
	  usymtab_supGlobalEntry (uue);      
	}

      sRef_clearGlobalScope ();

      voptgenerror (FLG_UNRECOG, message ("Unrecognized identifier: %s", s),
		    e->loc);
    }


  cstring_free (s);
  return (e);
}

exprNode
exprNode_iterExpr (/*@returned@*/ exprNode e)
{
  if (!processingIterVars ())
    {
      llcontbuglit ("checkIterParam: not in iter");
      return e;
    }
  
  if (uentry_isYield (uentryList_getN (uentry_getParams (getCurrentIter ()), 
				       iterParamNo ())))
    {
      if (exprNode_isDefined (e))
	{
	  if (fileloc_isDefined (e->loc))
	    {
	      voptgenerror
		(FLG_ITER,
		 message ("Yield parameter is not simple identifier: %s", 
			  exprNode_unparse (e)),
		 e->loc);
	    }
	  else
	    {
	      voptgenerror
		(FLG_ITER,
		 message ("Yield parameter is not simple identifier: %s",
			  exprNode_unparse (e)),
		 g_currentloc);
	      
	    }
	}
    }
  return e;
}

exprNode
exprNode_iterId (/*@observer@*/ uentry c)
{
  uentry ue;

  llassert (processingIterVars ());

  ue = uentryList_getN (uentry_getParams (getCurrentIter ()), 
			iterParamNo ());

  if (uentry_isYield (ue))
    {
      ctype ct = uentry_getType (ue);
      exprNode e = exprNode_createPlain (ct);
      cstring name = uentry_getName (c);
      uentry le = uentry_makeVariable (name, ct, fileloc_undefined, FALSE);

      uentry_setUsed (ue, g_currentloc);
      uentry_setHasNameError (ue); 
      
      cstring_free (name);
      
      e->kind = XPR_VAR;
      e->edata = exprData_makeId (le);
      e->loc = context_getSaveLocation ();
      e->sref = uentry_getSref (le);

      usymtab_supEntrySref (le);

      if (!context_inHeader ())
	{
	  if (optgenerror
	      (FLG_ITER,
	       message ("Yield parameter shadows local declaration: %q",
			uentry_getName (c)),
	       fileloc_isDefined (e->loc) ? e->loc : g_currentloc))
	    {
	      uentry_showWhereDeclared (c);
	    }
	}

      return e;
    }

  return (exprNode_fromIdentifierAux (c));
}

exprNode exprNode_iterStart (/*@observer@*/ uentry name, /*@only@*/ exprNodeList alist)
{
  exprNode ret = exprNode_create (ctype_unknown);

  ret->kind = XPR_ITERCALL;
  ret->edata = exprData_makeIterCall (name, alist);
  
  if (uentry_isIter (name))
    {
      uentryList params = uentry_getParams (name);

      if (context_inIterDef () 
	  && uentryList_size (params) == exprNodeList_size (alist))
	{
	  int i = 0;
	  
	  exprNodeList_elements (alist, arg)
	    {
	      uentry parg = uentryList_getN (params, i);

	      if (uentry_isYield (parg))
		{
		  uentry ue = exprNode_getUentry (arg);

		  if (uentry_isValid (ue))
		    {
		      ;
		    }
		}

	      i++;
	    } end_exprNodeList_elements;
	}

      (void) checkArgsReal (name, ret, params, alist, TRUE, ret);
      checkUnspecCall (ret, params, alist);
    }

  return ret;
}

/*@exposed@*/ sRef exprNode_getSref (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      /*@access sRef@*/
      if (e->sref == defref) /*@noaccess sRef@*/
	{
	  /*@-mods@*/
	  e->sref = sRef_makeUnknown (); 
          sRef_setAliasKind (e->sref, AK_ERROR, fileloc_undefined);
	  /*@=mods@*/
	  return e->sref;
	}
      else
	{
	  return e->sref;
	}
    }
  else
    {
      return sRef_undefined;
    }
}

/*@observer@*/ cstring
exprNode_unparseFirst (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      cstring ret;

      if (e->kind == XPR_STMTLIST
	  || e->kind == XPR_COMMA || e->kind == XPR_COND)
	{
	  exprNode first = exprData_getPairA (e->edata);

	  if (exprNode_isDefined (first))
	    {
	      return (exprNode_unparseFirst (exprData_getPairA (e->edata)));
	    }
	  else
	    {
	      return (cstring_makeLiteralTemp ("..."));
	    }
	}

      ret = cstring_elide (exprNode_unparse (e), 20);
      cstring_markOwned (ret);
      
      return (ret);
    }
  else
    {
      return cstring_makeLiteralTemp ("<error>");
    }
}

/*@observer@*/ cstring
exprNode_unparse (exprNode e)
{
  if (exprNode_isError (e))
    {
      return cstring_makeLiteralTemp ("<error>");
    }

  if (cstring_isDefined (e->etext))
    {
      return e->etext;
    }
  else
    {
      cstring ret = exprNode_doUnparse (e);

      /*@-modifies@*/ /* benevolent */
      e->etext = ret; 
      /*@=modifies@*/
      return ret;
    }
}

/*@observer@*/ fileloc
exprNode_loc (exprNode e)
{
  if (exprNode_isError (e))
    {
      return (g_currentloc);
    }
  else
    {
      return (e->loc);
    }
}

/*
** executes exprNode e
**    recursively rexecutes as though in original parse using
**    information in e->edata
*/

static /*@only@*/ exprNodeList exprNodeList_effect (exprNodeList e)
{
  exprNodeList ret = exprNodeList_new ();

  exprNodeList_elements (e, current)
    {
      exprNodeList_addh (ret, exprNode_effect (current));
    } end_exprNodeList_elements;

  return ret;
}

static /*@only@*/ exprNode exprNode_effect (exprNode e) 
   /*@globals internalState@*/
{
  bool innerEffect = inEffect;
  exprNode ret;
  exprData data;

  inEffect = TRUE;

  context_clearJustPopped ();

  if (exprNode_isError (e))
    {
      ret = exprNode_undefined;
    }
  else
    {
      /*
      ** Turn off expose and dependent transfer checking.
      ** Need to pass exposed internal nodes,
      ** [ copying would be a waste! ]
      ** [ Actually, I think I wasted a lot more time than its worth ]
      ** [ trying to do this. ]
      */

      /*@-exposetrans@*/
      /*@-observertrans@*/
      /*@-dependenttrans@*/
      
      data = e->edata;
      
      switch (e->kind)
	{
	case XPR_PARENS: 
	  ret = exprNode_addParens (exprData_getUopTok (data), 
				    exprNode_effect (exprData_getUopNode (data)));
	  break;
	case XPR_ASSIGN:
	  ret = exprNode_assign (exprNode_effect (exprData_getOpA (data)), 
				 exprNode_effect (exprData_getOpB (data)), 
				 exprData_getOpTok (data));
	  break;
	case XPR_INITBLOCK:
	  ret = exprNode_undefined;
	  break;
	case XPR_CALL:
	  ret = exprNode_functionCall (exprNode_effect (exprData_getFcn (data)),
				       exprNodeList_effect (exprData_getArgs (data)));
	  break;
	case XPR_EMPTY:
	  ret = e;
	  break;

	case XPR_LABEL:
	  ret = e;
	  break;

	case XPR_CONST:
	case XPR_VAR:
	  {
	    cstring id = exprData_getId (data);
	    uentry ue = usymtab_lookupSafe (id);

	    ret = exprNode_fromIdentifierAux (ue);
	    ret->loc = fileloc_update (ret->loc, e->loc);
	    break;
	  }
	case XPR_BODY:
	  ret = e;
	  break;
	case XPR_FETCH:
	  ret = exprNode_arrayFetch (exprNode_effect (exprData_getPairA (data)), 
				     exprNode_effect (exprData_getPairB (data)));
	  break;
	case XPR_OP:
	  ret = exprNode_op (exprNode_effect (exprData_getOpA (data)), 
			     exprNode_effect (exprData_getOpB (data)), 
			     exprData_getOpTok (data));
	  break;
	  
	case XPR_POSTOP:
	  ret = exprNode_postOp (exprNode_effect (exprData_getUopNode (data)), 
				 exprData_getUopTok (data));
	  break;
	case XPR_PREOP:
	  ret = exprNode_preOp (exprNode_effect (exprData_getUopNode (data)), 
				exprData_getUopTok (data));
	  break;

	case XPR_OFFSETOF:
	case XPR_SIZEOFT:
	case XPR_SIZEOF:
	case XPR_ALIGNOFT:
	case XPR_ALIGNOF:
	  ret = e;
	  break;
	  
	case XPR_VAARG:
	  ret = exprNode_vaArg (exprData_getCastTok (data),
				exprNode_effect (exprData_getCastNode (data)),
				exprData_getCastType (data));
	  break;
	  
	case XPR_CAST:
	  ret = exprNode_cast (exprData_getCastTok (data), 
			       exprNode_effect (exprData_getCastNode (data)), 
			       exprData_getCastType (data));
	  break;
	case XPR_ITERCALL:
	  ret = exprNode_iterStart (exprData_getIterCallIter (data),
				    exprNodeList_effect 
				    (exprData_getIterCallArgs (data)));
	  break;
	  
	case XPR_ITER:
	  ret = exprNode_iter (exprData_getIterSname (data),
			       exprNodeList_effect (exprData_getIterAlist (data)),
			       exprNode_effect (exprData_getIterBody (data)),
			       exprData_getIterEname (data));
	  break;
	  
	case XPR_FOR:
	  ret = exprNode_for (exprNode_effect (exprData_getPairA (data)), 
			      exprNode_effect (exprData_getPairB (data)));
	  break;
	  
	case XPR_FORPRED:
	  ret = exprNode_forPred (exprNode_effect (exprData_getTripleInit (data)),
				  exprNode_effect (exprData_getTripleTest (data)),
				  exprNode_effect (exprData_getTripleInc (data)));
	  break;
	  
	case XPR_TOK:
	  ret = exprNode_createTok (exprData_getTok (data));
	  break;
	  
	case XPR_GOTO:
	  ret = exprNode_goto (exprData_getLiteral (data));
	  ret->loc = fileloc_update (ret->loc, e->loc);
	  break;
	  
	case XPR_CONTINUE:
	  ret = exprNode_continue (exprData_getTok (data), QSAFEBREAK);
	  break;
	  
	case XPR_BREAK:
	  ret = exprNode_break (exprData_getTok (data), QSAFEBREAK);
	  break;
	  
	case XPR_RETURN:
	  ret = exprNode_return (exprNode_effect (exprData_getSingle (data)));
	  break;
	  
	case XPR_NULLRETURN:
	  ret = exprNode_nullReturn (exprData_getTok (data));
	  break;
	  
	case XPR_COMMA:
	  ret = exprNode_comma (exprNode_effect (exprData_getPairA (data)),
				exprNode_effect (exprData_getPairB (data)));
	  break;
	  
	case XPR_COND:
	  ret = exprNode_cond (exprNode_effect (exprData_getTriplePred (data)),
			       exprNode_effect (exprData_getTripleTrue (data)),
			       exprNode_effect (exprData_getTripleFalse (data)));
	  break;
	case XPR_IF:
	  ret = exprNode_if (exprNode_effect (exprData_getPairA (data)),
			     exprNode_effect (exprData_getPairB (data)));
	  break;
	  
	case XPR_IFELSE:
	  ret = exprNode_ifelse (exprNode_effect (exprData_getTriplePred (data)),
				 exprNode_effect (exprData_getTripleTrue (data)),
				 exprNode_effect (exprData_getTripleFalse (data)));
	  break;
	case XPR_WHILEPRED:
	  ret = exprNode_whilePred (exprData_getSingle (data));
	  break;
	  
	case XPR_WHILE:
	  ret = exprNode_while (exprNode_effect (exprData_getPairA (data)),
				exprNode_effect (exprData_getPairB (data)));
	  break;
	  
	case XPR_DOWHILE:
	  ret = exprNode_doWhile (exprNode_effect (exprData_getPairA (data)),
				  exprNode_effect (exprData_getPairB (data)));
	  break;

	case XPR_BLOCK:
	  ret = exprNode_makeBlock (exprNode_effect (exprData_getSingle (data)));
	  break;	  

	case XPR_STMT:
	  ret = exprNode_statement (exprNode_effect (exprData_getSingle (data)));
	  break;
	  
	case XPR_STMTLIST:
	  ret = exprNode_concat (exprNode_effect (exprData_getPairA (data)),
				 exprNode_effect (exprData_getPairB (data)));
	  break;
	  
	case XPR_FTCASE:
	case XPR_CASE:
	  ret = exprNode_caseMarker 
	    (exprNode_effect (exprData_getSingle (data)),
	     TRUE);
	  break;
	  
	case XPR_FTDEFAULT:
	case XPR_DEFAULT:
	  ret = exprNode_createTok (exprData_getTok (data));
	  break;
	  
	case XPR_SWITCH:
	  ret = exprNode_switch (exprNode_effect (exprData_getPairA (data)),
				 exprNode_effect (exprData_getPairB (data)));
	  break;
	  
	case XPR_INIT:
	  ret = exprNode_makeInitialization
	    (exprData_getInitId (data),
	     exprNode_effect (exprData_getInitNode (data)));
	  break;
	  
	case XPR_FACCESS:
	  ret = exprNode_fieldAccess (exprNode_effect (exprData_getFieldNode (data)),
				      cstring_copy (exprData_getFieldName (data)));
	  break;
	  
	case XPR_ARROW:
	  ret = exprNode_arrowAccess (exprNode_effect (exprData_getFieldNode (data)),
				      cstring_copy (exprData_getFieldName (data)));
	  break;
	  
	case XPR_STRINGLITERAL:
	  ret = e;
	  break;
	  
	case XPR_NUMLIT:
	  ret = e;
	  break;

	case XPR_NODE:
	  ret = e;
	  break;
	  /*@-branchstate@*/ 
	} 
      /*@=branchstate@*/
      /*@=observertrans@*/
      /*@=exposetrans@*/
      /*@=dependenttrans@*/
    }

  if (!innerEffect) 
    {
      inEffect = FALSE;
    }

  return ret;
}

static /*@observer@*/ cstring exprNode_rootVarName (exprNode e)
{
  cstring ret;
  exprData data;

  if (exprNode_isError (e))
    {
      return cstring_undefined;
    }

  data = e->edata;

  switch (e->kind)
    {
    case XPR_PARENS: 
      ret = exprNode_rootVarName (exprData_getUopNode (data));
      break;
    case XPR_ASSIGN:
      ret = exprNode_rootVarName (exprData_getOpA (data));
      break;
    case XPR_CONST:
    case XPR_VAR:
      ret = exprData_getId (data);
      break;
    case XPR_LABEL:
    case XPR_TOK:
    case XPR_ITERCALL:
    case XPR_EMPTY:
    case XPR_CALL:
    case XPR_INITBLOCK:
    case XPR_BODY:
    case XPR_FETCH:
    case XPR_OP:
    case XPR_POSTOP:
    case XPR_PREOP:
    case XPR_OFFSETOF:
    case XPR_ALIGNOFT:
    case XPR_ALIGNOF:
    case XPR_SIZEOFT:
    case XPR_SIZEOF:
    case XPR_VAARG:
    case XPR_CAST:
    case XPR_ITER:
    case XPR_FOR:
    case XPR_FORPRED:
    case XPR_BREAK:
    case XPR_RETURN:
    case XPR_NULLRETURN:
    case XPR_COMMA:
    case XPR_COND:
    case XPR_IF:
    case XPR_IFELSE:
    case XPR_WHILE:
    case XPR_WHILEPRED:
    case XPR_DOWHILE:
    case XPR_GOTO:
    case XPR_CONTINUE:
    case XPR_FTDEFAULT:
    case XPR_DEFAULT:
    case XPR_SWITCH:
    case XPR_FTCASE:
    case XPR_CASE:
    case XPR_BLOCK:
    case XPR_STMT:
    case XPR_STMTLIST:
    case XPR_INIT:
    case XPR_FACCESS:
    case XPR_ARROW:
    case XPR_NODE:
    case XPR_NUMLIT:
    case XPR_STRINGLITERAL:
      ret = cstring_undefined;
      break;
    }

  return ret;
}

static /*@only@*/ cstring exprNode_doUnparse (exprNode e)
{
  cstring ret;
  exprData data;

  if (exprNode_isError (e))
    {
      static /*@only@*/ cstring error = cstring_undefined;

      if (!cstring_isDefined (error))
	{
	  error = cstring_makeLiteral ("<error>");
	}
      
      return error;
    }

  data = e->edata;

  switch (e->kind)
    {
    case XPR_PARENS: 
      ret = message ("(%s)", exprNode_unparse (exprData_getUopNode (e->edata)));
      break;
    case XPR_ASSIGN:
      ret = message ("%s %s %s",
		     exprNode_unparse (exprData_getOpA (data)), 
		     lltok_unparse (exprData_getOpTok (data)),
		     exprNode_unparse (exprData_getOpB (data)));
      break;
    case XPR_CALL:
      ret = message ("%s(%q)",
		     exprNode_unparse (exprData_getFcn (data)), 
		     exprNodeList_unparse (exprData_getArgs (data)));
      break;
    case XPR_INITBLOCK:
      ret = message ("{ %q }", exprNodeList_unparse (exprData_getArgs (data)));
      break;
    case XPR_EMPTY:
      ret = cstring_undefined;
      break;
    case XPR_LABEL:
      ret = message ("%s:", exprData_getId (data));
      break;
    case XPR_CONST:
    case XPR_VAR:
      ret = cstring_copy (exprData_getId (data));
      break;
    case XPR_FETCH:
      ret = message ("%s[%s]", exprNode_unparse (exprData_getPairA (data)),
		     exprNode_unparse (exprData_getPairB (data)));
      break;
    case XPR_BODY:
      ret = message ("<body>");
      break;
    case XPR_OP:
      ret = message ("%s %s %s",
		     exprNode_unparse (exprData_getOpA (data)), 
		     lltok_unparse (exprData_getOpTok (data)),
      		     exprNode_unparse (exprData_getOpB (data))); 
      break;
      
    case XPR_PREOP: 
      ret = message ("%s%s",
		     lltok_unparse (exprData_getUopTok (data)),
		     exprNode_unparse (exprData_getUopNode (data))); 
      break;

    case XPR_POSTOP:
      ret = message ("%s%s",
		     exprNode_unparse (exprData_getUopNode (data)),
		     lltok_unparse (exprData_getUopTok (data))); 
      break;
      
    case XPR_OFFSETOF:
      ret = message ("offsetof(%s,%q)", 
		     ctype_unparse (qtype_getType (exprData_getOffsetType (data))),
		     cstringList_unparseSep (exprData_getOffsetName (data), cstring_makeLiteralTemp (".")));
      break;

    case XPR_SIZEOFT:
      ret = message ("sizeof(%s)", ctype_unparse (qtype_getType (exprData_getType (data))));
      break;
      
    case XPR_SIZEOF:
      ret = message ("sizeof(%s)", exprNode_unparse (exprData_getSingle (data)));
      break;

    case XPR_ALIGNOFT:
      ret = message ("alignof(%s)", ctype_unparse (qtype_getType (exprData_getType (data))));
      break;
      
    case XPR_ALIGNOF:
      ret = message ("alignof(%s)", exprNode_unparse (exprData_getSingle (data)));
      break;
      
    case XPR_VAARG:
      ret = message ("va_arg(%s, %q)", 
		     exprNode_unparse (exprData_getCastNode (data)),
		     qtype_unparse (exprData_getCastType (data)));
      break;
      
    case XPR_ITERCALL:
      ret = message ("%q(%q)", 
		     uentry_getName (exprData_getIterCallIter (data)),
		     exprNodeList_unparse (exprData_getIterCallArgs (data)));
      break;
    case XPR_ITER:
      ret = message ("%q(%q) %s %q",
		     uentry_getName (exprData_getIterSname (data)),
		     exprNodeList_unparse (exprData_getIterAlist (data)),
		     exprNode_unparse (exprData_getIterBody (data)),
		     uentry_getName (exprData_getIterEname (data)));
      break;
    case XPR_CAST:
      ret = message ("(%q)%s", 
		     qtype_unparse (exprData_getCastType (data)),
		     exprNode_unparse (exprData_getCastNode (data)));
      break;
      
    case XPR_FOR:
      ret = message ("%s %s", 
		     exprNode_unparse (exprData_getPairA (data)), 
		     exprNode_unparse (exprData_getPairB (data)));
      break;

    case XPR_FORPRED:
            ret = message ("for (%s; %s; %s)",
		     exprNode_unparse (exprData_getTripleInit (data)),
		     exprNode_unparse (exprData_getTripleTest (data)),
		     exprNode_unparse (exprData_getTripleInc (data)));
      break;
      
    case XPR_GOTO:
      ret = message ("goto %s", exprData_getLiteral (data));
      break;

    case XPR_CONTINUE:
      ret = cstring_makeLiteral ("continue");
      break;

    case XPR_BREAK:
      ret = cstring_makeLiteral ("break");
      break;

    case XPR_RETURN:
      ret = message ("return %s", exprNode_unparse (exprData_getSingle (data)));
      break;

    case XPR_NULLRETURN:
      ret = cstring_makeLiteral ("return");
      break;

    case XPR_COMMA:
      ret = message ("%s, %s", 
		     exprNode_unparse (exprData_getPairA (data)),
		     exprNode_unparse (exprData_getPairB (data)));
      break;
      
    case XPR_COND:
      ret = message ("%s ? %s : %s",
		     exprNode_unparse (exprData_getTriplePred (data)),
		     exprNode_unparse (exprData_getTripleTrue (data)),
		     exprNode_unparse (exprData_getTripleFalse (data)));
      break;
    case XPR_IF:
      ret = message ("if (%s) %s", 
		     exprNode_unparse (exprData_getPairA (data)),
		     exprNode_unparse (exprData_getPairB (data)));
      break;
      
    case XPR_IFELSE:
      ret = message ("if (%s) %s else %s",
		     exprNode_unparse (exprData_getTriplePred (data)),
		     exprNode_unparse (exprData_getTripleTrue (data)),
		     exprNode_unparse (exprData_getTripleFalse (data)));
      break;
    case XPR_WHILE:
      ret = message ("while (%s) %s",
		     exprNode_unparse (exprData_getPairA (data)),
		     exprNode_unparse (exprData_getPairB (data)));
      break;

    case XPR_WHILEPRED:
      ret = cstring_copy (exprNode_unparse (exprData_getSingle (data)));
      break;

    case XPR_TOK:
      ret = cstring_copy (lltok_unparse (exprData_getTok (data)));
      break;

    case XPR_DOWHILE:
      ret = message ("do { %s } while (%s)",
		     exprNode_unparse (exprData_getPairB (data)),
		     exprNode_unparse (exprData_getPairA (data)));
      break;
      
    case XPR_BLOCK:
      ret = message ("{ %s }", exprNode_unparseFirst (exprData_getSingle (data)));
      break;

    case XPR_STMT:
      ret = cstring_copy (exprNode_unparse (exprData_getSingle (data)));
      break;

    case XPR_STMTLIST:
      ret = message ("%s; %s", 
		     exprNode_unparse (exprData_getPairA (data)),
		     exprNode_unparse (exprData_getPairB (data)));
      break;
      
    case XPR_FTDEFAULT:
    case XPR_DEFAULT:
      ret = cstring_makeLiteral ("default:");
      break;

    case XPR_SWITCH:
      ret = message ("switch (%s) %s", 
		     exprNode_unparse (exprData_getPairA (data)),
		     exprNode_unparse (exprData_getPairB (data)));
      break;

    case XPR_FTCASE:
    case XPR_CASE:
      ret = message ("case %s:", 
		     exprNode_unparse (exprData_getSingle (data)));
      break;
      
    case XPR_INIT:
      ret = message ("%s = %s",
		     idDecl_getName (exprData_getInitId (data)),
		     exprNode_unparse (exprData_getInitNode (data)));
      break;
      
    case XPR_FACCESS:
      ret = message ("%s.%s",
		     exprNode_unparse (exprData_getFieldNode (data)),
		     exprData_getFieldName (data));
      break;
      
    case XPR_ARROW:
            ret = message ("%s->%s",
		     exprNode_unparse (exprData_getFieldNode (data)),
		     exprData_getFieldName (data));
      break;

    case XPR_STRINGLITERAL:
      ret = cstring_copy (exprData_getLiteral (data));
      break;

    case XPR_NUMLIT:
      ret = cstring_copy (exprData_getLiteral (data));
      break;

    case XPR_NODE:
      ret = cstring_makeLiteral ("<node>");
      break;
    }

  return ret;
}

bool 
exprNode_isCharLit (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return (multiVal_isChar (exprNode_getValue (e)));
    }
  else
    {
      return FALSE;
    }
}

bool
exprNode_isNumLit (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      return (multiVal_isInt (exprNode_getValue (e)));
    }
  else
    {
      return FALSE;
    }
}

static bool
exprNode_isFalseConstant (exprNode e)
{
  if (exprNode_isDefined (e))
    {
      cstring s = exprNode_rootVarName (e);

      if (cstring_equal (s, context_getFalseName ()))
	{
	  return TRUE;
	}
    }

  return FALSE;
}

bool
exprNode_matchLiteral (ctype expected, exprNode e)
{
  if (exprNode_isDefined (e))
    {
      multiVal m = exprNode_getValue (e);
      
      if (multiVal_isDefined (m))
	{
	  if (multiVal_isInt (m))
	    {
	      long int val = multiVal_forceInt (m);
	      
	      if (ctype_isDirectBool (ctype_realishType (expected)))
		{
		  if (val == 0) 
		    {
		      return FALSE; /* really?! return TRUE; allow use of 0 for FALSE */
		    }
		  else 
		    {
		      return FALSE;
		    }
		}
	      
	      if (ctype_isRealInt (expected))
		{
		  /*
		  ** unsigned <- [ constant >= 0 is okay ]
		  */
		  
		  if (ctype_isUnsigned (expected))
		    {
		      if (val < 0)
			{
			  return FALSE;
			}
		    }
		  
		  /*
		  ** No checks on sizes of integers...maybe add
		  ** these later.
		  */

		  DPRINTF (("Here: %s => %s", exprNode_unparse (e), ctype_unparse (expected)));
		  DPRINTF (("Type: %s / %s", ctype_unparse (exprNode_getType (e)),
			    bool_unparse (ctype_isInt (exprNode_getType (e)))));

		  if (context_getFlag (FLG_NUMLITERAL) 
		      && (ctype_isRegularInt (exprNode_getType (e)) || val == 0)) {
		    return TRUE;
		  } else {
		    if (val == 0) {
		      return TRUE;
		    } else {
		      return FALSE; /* evs 2000-05-17: previously, always returned TRUE */
		    }
		  }
		}
	      else if (ctype_isChar (expected))
		{
		  return FALSE;
		}
	      else if (ctype_isArrayPtr (expected))
		{
		  return (val == 0);
		}
	      else if (ctype_isAnyFloat (expected))
		{
		  return (context_getFlag (FLG_NUMLITERAL));
		}
	      else
		{
		  return FALSE;
		}
	    }
	  else if (multiVal_isDouble (m))
	    {
	      if (ctype_isAnyFloat (expected))
		{
		  return TRUE;
		}
	    }
	  else if (multiVal_isChar (m))
	    {
	      char val = multiVal_forceChar (m);	  
	      
	      if (ctype_isChar (expected))
		{
		  if (ctype_isUnsigned (expected) && ((int)val) < 0)
		    {
		      return FALSE;
		    }
		  else
		    {
		      return TRUE;
		    }
		}
	    }
	  else
	    {
	      return FALSE;
	    }
	}
    }
  
  return FALSE;
}

bool
exprNode_matchType (ctype expected, exprNode e)
{
  ctype actual;
  
  if (!exprNode_isDefined (e)) return TRUE;

  actual = ctype_realishType (exprNode_getType (e));

  if (ctype_match (ctype_realishType (expected), actual))
    {
      return TRUE;
    }

  llassert (!exprNode_isError (e));
  return (exprNode_matchLiteral (expected, e));
}

static bool
exprNode_matchTypes (exprNode e1, exprNode e2)
{
  ctype t1;
  ctype t2;
 
  if (!exprNode_isDefined (e1)) return TRUE;
  if (!exprNode_isDefined (e2)) return TRUE;

  /*
  ** realish type --- keep bools, bools 
  */ 

  t1 = ctype_realishType (exprNode_getType (e1));
  t2 = ctype_realishType (exprNode_getType (e2));

  if (ctype_match (t1, t2)) 
    {
      return TRUE;
    }

  return (exprNode_matchLiteral (t1, e2) || exprNode_matchLiteral (t2, e1));
}

/*
** pass e as ct
*/

static bool
  exprNode_matchArgType (ctype ct, exprNode e)
{
  ctype et;

  if (!exprNode_isDefined (e))
    {
      return TRUE;
    }

  et = ctype_realType (exprNode_getType (e));

  if (ctype_matchArg (ct, et)) return TRUE;
  
  llassert (!exprNode_isError (e));
  return (exprNode_matchLiteral (ct, e));
}

static /*@only@*/ exprNodeSList
  exprNode_flatten (/*@dependent@*/ exprNode e) /*@*/
{
  if (exprNode_isDefined (e))
    {
      if (e->kind == XPR_STMTLIST)
	{
	  return (exprNodeSList_append
		  (exprNode_flatten (exprData_getPairA (e->edata)),
		   exprNode_flatten (exprData_getPairB (e->edata))));
	}
      else if (e->kind == XPR_BLOCK)
	{
	  return (exprNode_flatten (exprData_getSingle (e->edata)));
	}
      else
	{
	  return (exprNodeSList_singleton (e));
	}
    }

  return exprNodeSList_new ();
}

static /*@exposed@*/ exprNode
exprNode_lastStatement (/*@returned@*/ exprNode e)
{
  if (exprNode_isDefined (e))
    {
      if (e->kind == XPR_STMTLIST)
	{
	  exprNode b = exprData_getPairB (e->edata);

	  if (exprNode_isDefined (b))
	    {
	      return exprNode_lastStatement (b);
	    }
	  else
	    {
	      return exprNode_lastStatement (exprData_getPairA (e->edata));
	    }
	}
      else if (e->kind == XPR_BLOCK)
	{
	  return (exprNode_lastStatement (exprData_getSingle (e->edata)));
	}
      else
	{
	  return (e);
	}
    }

  return exprNode_undefined;
}

static /*@exposed@*/ exprNode
exprNode_firstStatement (/*@returned@*/ exprNode e)
{
  if (exprNode_isDefined (e))
    {
      if (e->kind == XPR_STMTLIST)
	{
	  exprNode b = exprData_getPairA (e->edata);

	  if (exprNode_isDefined (b))
	    {
	      return exprNode_firstStatement (b);
	    }
	  else
	    {
	      return exprNode_firstStatement (exprData_getPairB (e->edata));
	    }
	}
      else if (e->kind == XPR_BLOCK)
	{
	  return (exprNode_firstStatement (exprData_getSingle (e->edata)));
	}
      else
	{
	  return (e);
	}
    }

  return exprNode_undefined;
}
  
static void
exprNode_mergeUSs (exprNode res, exprNode other)
{
  if (exprNode_isDefined (res) && exprNode_isDefined (other))
    {
      res->msets = sRefSet_union (res->msets, other->msets);
      res->sets = sRefSet_union (res->sets, other->sets);
      res->uses = sRefSet_union (res->uses, other->uses);
    }
}

static void
exprNode_mergeCondUSs (exprNode res, exprNode other1, exprNode other2)
{
  if (exprNode_isDefined (res))
    {
      if (exprNode_isDefined (other1))
	{
	  res->sets = sRefSet_union (res->sets, other1->sets);
	  res->msets = sRefSet_union (res->msets, other1->msets);
	  res->uses = sRefSet_union (res->uses, other1->uses);
	}
      if (exprNode_isDefined (other2))
	{
	  res->sets = sRefSet_union (res->sets, other2->sets);
	  res->msets = sRefSet_union (res->msets, other2->msets);
	  res->uses = sRefSet_union (res->uses, other2->uses);
	}
    }
}

/*
** modifies e->uses
**
** Reports errors is s is not defined.
*/

static void
exprNode_addUse (exprNode e, sRef s)
{
  if (exprNode_isDefined (e))
    {
      e->uses = sRefSet_insert (e->uses, s);
    }
}
  
void
exprNode_checkUse (exprNode e, sRef s, fileloc loc)
{
  if (sRef_isKnown (s) && !sRef_isConst (s))
    {
      /*
      ** need to check all outer types are useable
      */

      DPRINTF (("Check use: %s / %s",
		exprNode_unparse (e), sRef_unparse (s)));

      exprNode_addUse (e, s);
     
      if (!context_inProtectVars ())
	{
	  /*
	  ** only report the deepest error
	  */
	  
	  sRef errorRef = sRef_undefined;
	  sRef lastRef  = sRef_undefined;
	  bool deadRef = FALSE;
	  bool unuseable = FALSE;
	  bool errorMaybe = FALSE;
	  
	  while (sRef_isValid (s) && sRef_isKnown (s))
	    {
	      ynm readable = sRef_isReadable (s);

	      if (!(ynm_toBoolStrict (readable)))
		{
		  if (ynm_isMaybe (readable))
		    {
		      lastRef = errorRef;
		      errorRef = s;
		      deadRef = sRef_isPossiblyDead (errorRef);
		      unuseable = sRef_isUnuseable (errorRef);
		      errorMaybe = TRUE;
		    }
		  else
		    {
		      lastRef = errorRef;
		      errorRef = s;
		      deadRef = sRef_isDead (errorRef);
		      unuseable = sRef_isUnuseable (errorRef);
		      errorMaybe = FALSE;
		    }

		  if (!sRef_isPartial (s))
		    {
		      sRef_setDefined (s, fileloc_undefined);
		    }
		}

	      s = sRef_getBaseSafe (s);
	    } /* end while */
	  
	  if (sRef_isValid (errorRef)) 
	    {
	      if (sRef_isValid (lastRef) && sRef_isField (lastRef) 
		  && sRef_isPointer (errorRef))
		{
		  errorRef = lastRef;
		}
	      
	      if (deadRef)
		{
		  if (sRef_isThroughArrayFetch (errorRef))
		    {
		      if (optgenerror 
			  (FLG_STRICTUSERELEASED,
			   message ("%q %q may be used after being released", 
				    sRef_unparseKindNamePlain (errorRef),
				    sRef_unparse (errorRef)),
			   loc))
			{
			  sRef_showRefKilled (errorRef);
			  
			  if (sRef_isKept (errorRef))
			    {
			      sRef_clearAliasState (errorRef, loc);
			    }
			}
		    }
		  else
		    {
		      DPRINTF (("HERE: %s", sRef_unparse (errorRef)));

		      if (optgenerror
			  (FLG_USERELEASED,
			   message ("%q %q %qused after being released", 
				    sRef_unparseKindNamePlain (errorRef),
				    sRef_unparse (errorRef),
				    cstring_makeLiteral (errorMaybe 
							 ? "may be " : "")),
			   loc))
			{
			  sRef_showRefKilled (errorRef);
			  
			  if (sRef_isKept (errorRef))
			    {
			      sRef_clearAliasState (errorRef, loc);
			    }
			}
		    }
		}
	      else if (unuseable)
		{
		  if (optgenerror
		      (FLG_USEDEF,
		       message ("%q %q%qused in inconsistent state", 
				sRef_unparseKindName (errorRef),
				sRef_unparseOpt (errorRef),
				cstring_makeLiteral (errorMaybe ? "may be " : "")),
		       loc))
		    {
		      sRef_showStateInconsistent (errorRef);
		    }
		}
	      else
		{
		  DPRINTF (("HERE: %s", sRef_unparse (errorRef)));

		  voptgenerror 
		    (FLG_USEDEF,
		     message ("%q %q%qused before definition", 
			      sRef_unparseKindName (errorRef),
			      sRef_unparseOpt (errorRef),
			      cstring_makeLiteral (errorMaybe ? "may be " : "")),
		     loc);
		}
	      
	      sRef_setDefined (errorRef, loc);
	  
	      if (sRef_isAddress (errorRef))
		{
		  sRef_setDefined (sRef_getRootBase (errorRef), loc);
		}
	    } /* end is error */
	}
    }

  setCodePoint ();
}

static void
checkSafeUse (exprNode e, sRef s)
{
  if (exprNode_isDefined (e) && sRef_isKnown (s))
    {
      e->uses = sRefSet_insert (e->uses, s);
    }
}

static void
exprNode_checkSetAny (exprNode e, /*@dependent@*/ cstring name)
{
  if (exprNode_isDefined (e))
    {
      e->sets = sRefSet_insert (e->sets, sRef_makeUnconstrained (name));
    }
}

void
exprNode_checkSet (exprNode e, sRef s)
{
  sRef defines = sRef_undefined;

  if (sRef_isValid (s) && !sRef_isNothing (s))
    {
      uentry ue = sRef_getBaseUentry (s);

      if (uentry_isValid (ue))
	{
	  uentry_setLset (ue);
	}

      if (!ynm_toBoolStrict (sRef_isWriteable (s)))
	{
	  voptgenerror (FLG_USEDEF,
			message ("Attempt to set unuseable storage: %q", 
				 sRef_unparse (s)),
			exprNode_loc (e));
	}
     
      if (sRef_isMeaningful (s))
	{
	  
	  if (sRef_isDead (s))
	    {
	      sRef base = sRef_getBaseSafe (s);

	      if (sRef_isValid (base) 
		  && sRef_isDead (base))
		{
		  sRef_setPartial (s, exprNode_loc (e));
		}
	      
	      defines = s; /* okay - modifies for only param */
	    }
	  else if (sRef_isPartial (s))
	    {
	      sRef eref = exprNode_getSref (e);

	      if (!sRef_isPartial (eref))
		{
		  /*
	          ** should do something different here???
		  */
		  
		  sRef_setDefinedComplete (eref, exprNode_loc (e));		  
		}
	      else
		{
		  sRef_setPartialDefinedComplete (eref, exprNode_loc (e));
		}

	      if (sRef_isMeaningful (eref))
		{
		  defines = eref;
		}
	      else
		{		 
		  defines = s;
		}
	    }
	  else if (sRef_isAllocated (s))
	    {
	      sRef eref = exprNode_getSref (e);

	      
	      if (!sRef_isAllocated (eref))
		{
		  sRef_setDefinedComplete (eref, exprNode_loc (e));
		}
	      else
		{
		  sRef base = sRef_getBaseSafe (eref);
		  
		  if (sRef_isValid (base))
		    {
		      sRef_setPdefined (base, exprNode_loc (e)); 
		    }
		}

	      defines = s;
	    }
	  else 
	    {
	      sRef_setDefinedNCComplete (s, exprNode_loc (e));
	      defines = s;
	    }

	}
      else /* not meaningful...but still need to insert it */
	{
	  defines = s;
	}
    }

  if (exprNode_isDefined (e) && sRef_isValid (defines))
    {
      e->sets = sRefSet_insert (e->sets, defines); 
    }
}

void
exprNode_checkMSet (exprNode e, sRef s)
{
  if (sRef_isValid (s) && !sRef_isNothing (s))
    {
      uentry ue = sRef_getBaseUentry (s);

      if (uentry_isValid (ue))
	{
	  uentry_setLset (ue);
	}

      if (!ynm_toBoolStrict (sRef_isWriteable (s)))
	{
	  voptgenerror (FLG_USEDEF,
			message ("Attempt to set unuseable storage: %q", sRef_unparse (s)),
			exprNode_loc (e));
	}
      
      if (sRef_isMeaningful (s))
	{
	  sRef_setDefinedComplete (s, exprNode_loc (e));
	}
      
      if (exprNode_isDefined (e))
	{
	  e->msets = sRefSet_insert (e->msets, s);
	}
    }
}

static void
checkUnspecCall (/*@notnull@*/ /*@dependent@*/ exprNode fcn, uentryList params, exprNodeList args)
{
  checkAnyCall (fcn, cstring_undefined, params, args, 
		FALSE, sRefSet_undefined, FALSE, 0);
}

static void
checkOneArg (uentry ucurrent, /*@notnull@*/ exprNode current, 
	     /*@dependent@*/ exprNode fcn, bool isSpec, int argno, int totargs)
{
  setCodePoint ();
  
  if (uentry_isYield (ucurrent))
    {
      sRef_setDefined (exprNode_getSref (current), exprNode_loc (current));
      exprNode_checkSet (current, current->sref);
    }
  else 
    {
      if (uentry_isSefParam (ucurrent))
	{
	  sRefSet sets = current->sets;
	  sRef ref = exprNode_getSref (current);

	  if (sRef_isMacroParamRef (ref))
	    {
	      uentry ue = sRef_getUentry (ref);

	      if (!uentry_isSefParam (ue))
		{
		  voptgenerror 
		    (FLG_SEFPARAMS,
		     message
		     ("Parameter %d to %s is declared sef, but "
		      "the argument is a macro parameter declared "
		      "without sef: %s",
		      argno, exprNode_unparse (fcn),
		      exprNode_unparse (current)),
		     exprNode_loc (current));
		}
	    }

	  if (!sRefSet_isEmpty (sets))
	    {
	      sRefSet reported = sRefSet_undefined;
	      
	      sRefSet_realElements (current->sets, el)
		{
		  if (sRefSet_isSameNameMember (reported, el))
		    {
		      ; /* don't report again */
		    }
		  else
		    {
		      if (sRef_isUnconstrained (el))
			{
			  voptgenerror 
			    (FLG_SEFUNSPEC,
			     message
			     ("Parameter %d to %s is declared sef, but "
			      "the argument calls unconstrained function %s "
			      "(no guarantee it will not modify something): %s",
			      argno, exprNode_unparse (fcn),
			      sRef_unconstrainedName (el),
			      exprNode_unparse (current)),
			     exprNode_loc (current));
			}
		      else
			{
			  voptgenerror 
			    (FLG_SEFPARAMS,
			     message
			     ("Parameter %d to %s is declared sef, but "
			      "the argument may modify %q: %s",
			      argno, exprNode_unparse (fcn),
			      sRef_unparse (el),
			      exprNode_unparse (current)),
			     exprNode_loc (current));
			}
		    } 
		} end_sRefSet_realElements;
	    }
	}
      
      checkPassTransfer (current, ucurrent, isSpec, fcn, argno, totargs);
      exprNode_mergeUSs (fcn, current);
    }
}

static void
  checkAnyCall (/*@dependent@*/ exprNode fcn, 
		/*@dependent@*/ cstring fname,
		uentryList pn, 
		exprNodeList args, 
		bool hasMods, sRefSet mods,
		bool isSpec,
		int specialArgs)
{
  int paramno = 0;
  int nargs = exprNodeList_size (args);

  setCodePoint ();

  /*
  ** concat all args ud's to f, add each arg sref as a use unless
  ** it was specified as "out", in which case it is a def.
  */
  
  uentryList_reset (pn);
  
  /*
  ** aliasing checks:
  **
  **    if paramn is only or unique, no other arg may alias argn
  */
  
  exprNodeList_elements (args, current) 
    {
      paramno++;
      
      if (exprNode_isDefined (current)) 
	{
	  if ((!uentryList_isUndefined (pn) && !uentryList_isFinished (pn))) 
	    {
	      uentry ucurrent = uentryList_current (pn);
	      
	      if (specialArgs == 0 
		  || (paramno < specialArgs))
		{
		  		  checkOneArg (ucurrent, current, fcn, isSpec, paramno, nargs);

		  if (context_maybeSet (FLG_ALIASUNIQUE))
		    {
		      if (uentry_isOnly (ucurrent)
			  || uentry_isUnique (ucurrent))
			{
			  checkUniqueParams (fcn, current, args,
					     paramno, ucurrent);
			}
		    }
		} 
	    }
	  else /* uentry is undefined */
	    {
	      if (specialArgs == 0)
		{
		  exprNode_checkUseParam (current);
		}

	      exprNode_mergeUSs (fcn, current);
	    }	
	}
      uentryList_advanceSafe (pn);
    } end_exprNodeList_elements;
  
  if (hasMods)
    {
      setCodePoint ();

      sRefSet_allElements (mods, s)
	{
	  sRef fb;
	  sRef rb = sRef_getRootBase (s);
	  
	  if (sRef_isGlobal (rb))
	    {
	      context_usedGlobal (rb);
	    }
	  
	  fb = sRef_fixBaseParam (s, args);
	  
	  if (!sRef_isMacroParamRef (fb))
	    {
	      if (sRef_isNothing (fb))
		{
		  ;
		}
	      else
		{
		  if (sRef_isValid (fb))
		    {
		      uentry ue = sRef_getBaseUentry (s);
		      
		      if (uentry_isValid (ue))
			{
			  uentry_setLset (ue);
			}
		    }
		  
		  fcn->sets = sRefSet_insert (fcn->sets, fb);
		}
	    }
	  sRef_clearDerivedComplete (s); 
	} end_sRefSet_allElements;
      
      setCodePoint ();
    }
  else
    {
      if (context_hasMods ())
	{
	  if (context_maybeSet (FLG_MODUNCON))
	    {
	      voptgenerror
		(FLG_MODUNCON,
		 message ("Undetected modification possible "
			  "from call to unconstrained function %s: %s", 
			  fname,
			  exprNode_unparse (fcn)),
		 exprNode_loc (fcn));
	    }
	}
      else
	{
	  if (context_maybeSet (FLG_MODUNCONNOMODS)
	      && !(context_inIterDef () || context_inIterEnd ()))
	    {
	      voptgenerror
		(FLG_MODUNCONNOMODS,
		 message ("Undetected modification possible "
			  "from call to unconstrained function %s: %s", 
			  fname,
			  exprNode_unparse (fcn)),
		 exprNode_loc (fcn));
	    }
	}

      exprNode_checkSetAny (fcn, fname);
    }
}

void exprNode_checkUseParam (exprNode current)
{
  if (exprNode_isDefined (current))
    {
      exprNode_checkUse (current, current->sref, current->loc);
    }
}

static ctype
  checkNumerics (ctype tr1, ctype tr2, ctype te1, ctype te2,
		 /*@notnull@*/ exprNode e1, /*@notnull@*/ exprNode e2,
		 lltok op)
{
  ctype ret = tr1;
  
  if (!ctype_match (tr1, tr2))
    {
      if ((ctype_isRealInt (tr1) || ctype_isReal (tr1)) &&
	  (ctype_isRealInt (tr2) || ctype_isReal (tr2)))
	{
	  ;
	}
      else
	{
	  (void) gentypeerror 
	    (tr1, e1, tr2, e2,
	     message ("Incompatible types for %s (%s, %s): %s %s %s",
		      lltok_unparse (op),
		      ctype_unparse (te1),
		      ctype_unparse (te2),
		      exprNode_unparse (e1), lltok_unparse (op), 
		      exprNode_unparse (e2)),
	     e1->loc);
	}
      ret = ctype_unknown;
    }
  else
    {
      if (ctype_isForceRealNumeric (&tr1) && ctype_isForceRealNumeric (&tr2))
	{
	  ret = ctype_resolveNumerics (tr1, tr2);
	}
      else if (!context_msgStrictOps ()) 
	{
	  	  if (ctype_isPointer (tr1))
	    {
	      if (ctype_isPointer (tr2) && !exprNode_isNullValue (e2))
		{
		  ret = ctype_int;
		}
	      else if (ctype_isInt (tr2))
		{
		  ret = te1;
		}
	      else
		{
		  ret = ctype_unknown;
		}
	    }
	  else if (ctype_isPointer (tr2))
	    {
	      if (ctype_isPointer (tr1))
		{
		  ret = ctype_int;
		}
	      else if (ctype_isInt (tr1))
		{
		  ret = te2;
		}
	      else
		{
		  ret = ctype_unknown; 
		}
	    }
	  else
	    {
	      ret = ctype_resolveNumerics (tr1, tr2);
	    }
	}
      else
	{
	  int opid = lltok_getTok (op);
	  bool comparop = (opid == EQ_OP || opid == NE_OP 
			   || opid == TLT || opid == TGT
			   || opid == LE_OP || opid == GE_OP);
	  
	  if (!ctype_isNumeric (tr1) && !ctype_isNumeric (tr2))
	    {
	      if (comparop
		  && ((ctype_isEnum (tr1) && ctype_isEnum (tr2))
		      || (ctype_isBool (tr1) && ctype_isBool (tr2))
		      || (ctype_isChar (tr1) && ctype_isChar (tr2))))
		{
		  ; /* no error */
		}
	      else
		{
		  if (ctype_sameName (te1, te2))
		    {
		      voptgenerror
			(FLG_STRICTOPS,
			 message ("Operands of %s are non-numeric (%t): %s %s %s",
				  lltok_unparse (op), te1, 
				  exprNode_unparse (e1), lltok_unparse (op), 
				  exprNode_unparse (e2)),
			 e1->loc);
		    }
		  else
		    {
		      voptgenerror
			(FLG_STRICTOPS,
			 message ("Operands of %s are non-numerics (%t, %t): %s %s %s",
				  lltok_unparse (op), te1, te2, 
				  exprNode_unparse (e1), lltok_unparse (op),
				  exprNode_unparse (e2)),
			 e1->loc);
		    }
		}
	    }
	  else if (!ctype_isNumeric (tr1))
	    {
	      voptgenerror
		(FLG_STRICTOPS,
		 message ("Right operand of %s is non-numeric (%t): %s %s %s",
			  lltok_unparse (op), te1, 
			  exprNode_unparse (e1), lltok_unparse (op), 
			  exprNode_unparse (e2)),
		 e1->loc);
	    }
	  else 
	    {
	      if (!ctype_isNumeric (tr2))
		{
		  voptgenerror
		    (FLG_STRICTOPS,
		     message ("Left operand of %s is non-numeric (%t): %s %s %s",
			      lltok_unparse (op), te2, 
			      exprNode_unparse (e1), lltok_unparse (op), 
			      exprNode_unparse (e2)),
		     e2->loc);
		}
	    }
	  
	  ret = ctype_unknown;
	}
    }

  return ret;
}

static void
abstractOpError (ctype tr1, ctype tr2, lltok op, 
		 /*@notnull@*/ exprNode e1, /*@notnull@*/ exprNode e2, 
		 fileloc loc1, fileloc loc2)
{
  if (ctype_isRealAbstract (tr1) && ctype_isRealAbstract (tr2))
    {
      if (ctype_match (tr1, tr2))
	{
	  voptgenerror
	    (FLG_ABSTRACT,
	     message ("Operands of %s are abstract type (%t): %s %s %s",
		      lltok_unparse (op), tr1, 
		      exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	     loc1);
	}
      else
	{
	  voptgenerror 
	    (FLG_ABSTRACT,
	     message ("Operands of %s are abstract types (%t, %t): %s %s %s",
		      lltok_unparse (op), tr1, tr2, 
		      exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	     loc1);
	}
    }
  else if (ctype_isRealAbstract (tr1))
    {
      voptgenerror
	(FLG_ABSTRACT,
	 message ("Left operand of %s is abstract type (%t): %s %s %s",
		  lltok_unparse (op), tr1, 
		  exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	 loc1);
    }
  else 
    {
      if (ctype_isRealAbstract (tr2))
	{
	  voptgenerror
	    (FLG_ABSTRACT,
	     message ("Right operand of %s is abstract type (%t): %s %s %s",
		      lltok_unparse (op), tr2, 
		      exprNode_unparse (e1), lltok_unparse (op), exprNode_unparse (e2)),
	     loc2);
	}
    }
}

/*
** e1 <= e2
**
** requies e1 and e2 and not error exprNode's.
**
** Checks:
**
**    If e1 is a component of an abstract type, and e2 is mutable and client-visible, 
**    the rep of the abstract type is exposed.
**
** The order is very important:
**
**    check rep expose (move into check transfer)
**    check transfer
**    setup aliases
*/

/*
** This isn't really a sensible procedure, but the indententation
** was getting too deep.
*/

static void
checkOneRepExpose (sRef ysr, sRef base, 
		   /*@notnull@*/ exprNode e1, 
		   /*@notnull@*/ exprNode e2, ctype ct,
		   sRef s2b)
{
  if (!(sRef_isOnly (ysr) || sRef_isKeep (ysr) 
	|| sRef_isOwned (ysr) || sRef_isExposed (ysr)))
    {
      if (sRef_isAnyParam (base) && !sRef_isExposed (base))
	{
	  if (sRef_isIReference (ysr))
	    {
	      if (sRef_sameName (base, sRef_getRootBase (e2->sref)))
		{
		  voptgenerror 
		    (FLG_ASSIGNEXPOSE,
		     message
		     ("Assignment of mutable component of parameter %q "
		      "to component of abstract "
		      "type %s exposes rep: %s = %s",
		      sRef_unparse (base),
		      ctype_unparse (ct),
		      exprNode_unparse (e1), exprNode_unparse (e2)),
		     e1->loc);
		}
	      else
		{
		  voptgenerror 
		    (FLG_ASSIGNEXPOSE,
		     message
		     ("Assignment of mutable component of parameter %q "
		      "(through alias %q) to component of abstract "
		      "type %s exposes rep: %s = %s",
		      sRef_unparse (base),
		      sRef_unparse (e2->sref),
		      ctype_unparse (ct),
		      exprNode_unparse (e1), exprNode_unparse (e2)),
		     e1->loc);
		}
	    }
	  else
	    {
	      if (sRef_sameName (base, sRef_getRootBase (e2->sref)))
		{
		  voptgenerror 
		    (FLG_ASSIGNEXPOSE,
		     message ("Assignment of mutable parameter %q "
			      "to component of abstract type %s "
			      "exposes rep: %s = %s",
			      sRef_unparse (base),
			      ctype_unparse (ct),
			      exprNode_unparse (e1), 
			      exprNode_unparse (e2)),
		     e1->loc);
		}
	      else
		{
		  voptgenerror 
		    (FLG_ASSIGNEXPOSE,
		     message ("Assignment of mutable parameter %q "
			      "(through alias %q) to "
			      "component of abstract type %s exposes "
			      "rep: %s = %s",
			      sRef_unparse (base),
			      sRef_unparse (e2->sref),
			      ctype_unparse (ct),
			      exprNode_unparse (e1), 
			      exprNode_unparse (e2)),
		     e1->loc);
		}
	    }
	}
      
      if (sRef_isGlobal (s2b))
	{
	  if (sRef_sameName (base, sRef_getRootBase (e2->sref)))
	    {
	      voptgenerror 
		(FLG_REPEXPOSE,
		 message ("Assignment of global %q "
			  "to component of "
			  "abstract type %s exposes rep: %s = %s",
			  sRef_unparse (base),
			  ctype_unparse (ct),
			  exprNode_unparse (e1), exprNode_unparse (e2)),
		 e1->loc);
	    }
	  else
	    {
	      voptgenerror 
		(FLG_REPEXPOSE,
		 message ("Assignment of global %q (through alias %q) "
			  "to component of "
			  "abstract type %s exposes rep: %s = %s",
			  sRef_unparse (base),
			  sRef_unparse (e2->sref),
			  ctype_unparse (ct),
			  exprNode_unparse (e1), exprNode_unparse (e2)),
		 e1->loc);
	    }
	}
    }
}

static void
doAssign (/*@notnull@*/ exprNode e1, /*@notnull@*/ exprNode e2, bool isInit)
{
  if (ctype_isRealFunction (exprNode_getType (e1))
      && !ctype_isRealPointer (exprNode_getType (e1)))
    {
      voptgenerror 
	(FLG_TYPE,
	 message ("Invalid left-hand side of assignment (function type %s): %s",
		  ctype_unparse (exprNode_getType (e1)),
		  exprNode_unparse (e1)),
	 e1->loc);
    }

  if (context_getFlag (FLG_ASSIGNEXPOSE) && ctype_isMutable (e2->typ))
    {
      ctype t2 = exprNode_getType (e2);
      sRef sr = sRef_getRootBase (e1->sref);
      ctype ct = sRef_getType (sr);

      if (ctype_isAbstract (t2) 
	  && !(uentry_isMutableDatatype (usymtab_getTypeEntry (ctype_typeId (t2)))))
	{
	  /* it is immutable, okay to reference */
	  goto donerepexpose;
	}

      if (ctype_isAbstract (ct) && sRef_isIReference (e1->sref))
	{
	  sRef s2b = sRef_getRootBase (e2->sref);
	  sRef s1 = e1->sref;
	  sRef s1b = sRef_getRootBase (s1);
	  sRefSet aliases;

	  aliases = usymtab_canAlias (e2->sref);
	  
	  if (!sRef_similar (s2b, s1b) 
	      && !sRef_isExposed (s1)
	      && !(sRef_isOnly (s2b) || sRef_isKeep (s2b) || sRef_isExposed (s2b)))
	    {
	      if (sRef_isAnyParam (s2b) && !sRef_isOnly (s2b) 
		  && !sRef_isOwned (s2b) && !sRef_isKeep (s2b)
		  && !sRef_isExposed (s2b))
		{
		  if (sRef_isIReference (e2->sref))
		    {
		      voptgenerror 
			(FLG_ASSIGNEXPOSE,
			 message 
			 ("Assignment of mutable component of parameter %q "
			  "to component of abstract type %s exposes rep: %s = %s",
			  sRef_unparse (s2b),
			  ctype_unparse (ct),
			  exprNode_unparse (e1), exprNode_unparse (e2)),
			 e1->loc);
		    }
		  else
		    {
		      voptgenerror 
			(FLG_ASSIGNEXPOSE,
			 message ("Assignment of mutable parameter %q to "
				  "component of abstract type %s exposes rep: %s = %s",
				  sRef_unparse (s2b),
				  ctype_unparse (ct),
				  exprNode_unparse (e1), exprNode_unparse (e2)),
			 e1->loc);
		    }
		}

	      if (sRef_isGlobal (s2b))
		{
		  voptgenerror
		    (FLG_ASSIGNEXPOSE,
		     message ("Assignment of global %q to component of "
			      "abstract type %s exposes rep: %s = %s",
			      sRef_unparse (s2b),
			      ctype_unparse (ct),
			      exprNode_unparse (e1), exprNode_unparse (e2)),
		     e1->loc);
		}
	      
	      sRefSet_realElements (aliases, ysr)
		{
		  sRef base = sRef_getRootBase (ysr);
		  
		  if (sRef_similar (ysr, s2b) || sRef_similar (s1b, base)
		      || sRef_sameName (base, s1b))
		    {
		     ; /* error already reported or same sref */
		    }
		  else
		    {
		      checkOneRepExpose (ysr, base, e1, e2, ct, s2b);
		    }
		} end_sRefSet_realElements;
	    }
	  sRefSet_free (aliases);
	}
    }

 donerepexpose:

  /*
  ** function variables don't really work...
  */

  if (!ctype_isFunction (ctype_realType (e2->typ)))
    {
      if (isInit)
	{
	  checkInitTransfer (e1, e2); 
	}
      else
	{
	  checkAssignTransfer (e1, e2); 
	}
    }
  else
    {
      sRef fref = e2->sref;

      sRef_setDefState (e1->sref, sRef_getDefState (fref), e1->loc);
      sRef_setNullState (e1->sref, sRef_getNullState (fref), e1->loc);

            /* Need to typecheck the annotation on the parameters */
      
      if (ctype_isRealFunction (e1->typ)) {
	uentryList e1p = ctype_argsFunction (ctype_realType (e1->typ));
	uentryList e2p = ctype_argsFunction (ctype_realType (e2->typ));

	if (!uentryList_isMissingParams (e1p)
	    && !uentryList_isMissingParams (e2p)
	    && uentryList_size (e1p) > 0) {
	  if (uentryList_size (e1p) == uentryList_size (e2p)) {
	    int n = 0;
	    
	    uentryList_elements (e1p, el1) {
	      uentry el2;

	      el2 = uentryList_getN (e2p, n);
	      n++;
	      uentry_checkMatchParam (el1, el2, n, e2);
	    } end_uentryList_elements;
	  }
	}
      }
    }

  if (isInit && sRef_isGlobal (e1->sref))
    {
       ;
    }
  else
    {
      updateAliases (e1, e2); 
    }
}

static void 
checkMacroParen (exprNode e)
{
  if (exprNode_isError (e) || e->kind == XPR_CAST)
    {
     ;
    }
  else 
    {
      if (sRef_isUnsafe (e->sref) && !exprNode_isInParens (e))
	{
	  voptgenerror 
	    (FLG_MACROPARENS,
	     message ("Macro parameter used without parentheses: %s", 
		      exprNode_unparse (e)),
	     e->loc);
	}
    }
}

static void
reflectNullTest (/*@notnull@*/ exprNode e, bool isnull)
{
  if (isnull)
    {
      e->guards = guardSet_addTrueGuard (e->guards, e->sref);
    }
  else
    {
      e->guards = guardSet_addFalseGuard (e->guards, e->sref);
    }
}

/*
** e1 <= e2
**
** if e2 is a parameter or global derived location which
** can be modified (that is, e2 is a mutable abstract type,
** or a derived pointer), then e1 can alias e2.
**
** e1 can alias everything which e2 can alias.
**
** Also, if e1 is guarded, remove from guard sets!
*/

static void updateAliases (/*@notnull@*/ exprNode e1, /*@notnull@*/ exprNode e2)
{
  if (!context_inProtectVars ())
    {
      /*
      ** depends on types of e1 and e2
      */
      
      sRef s1 = e1->sref;
      sRef s2 = e2->sref;
      ctype t1 = exprNode_getType (e1);
      
      /* handle pointer sRefs, record fields, arrays, etc... */
      
      if (!ctype_isRealSU (t1))
	{
	  sRef_copyRealDerivedComplete (s1, s2);
	}

      if (ctype_isMutable (t1) && sRef_isKnown (s1))
	{
	  usymtab_clearAlias (s1);
	  usymtab_addMustAlias (s1, s2); 
	}

      if (sRef_possiblyNull (s1) && usymtab_isGuarded (s1))
	{
	  usymtab_unguard (s1);
	}
    }
}

exprNode exprNode_updateLocation (/*@returned@*/ exprNode e, /*@temp@*/ fileloc loc)
{
  if (exprNode_isDefined (e))
    {
      e->loc = fileloc_update (e->loc, loc);
    }
  else
    {
      e = exprNode_createLoc (ctype_unknown, fileloc_copy (loc));
    }

  return (e);
}

static void checkUniqueParams (exprNode fcn,
			       /*@notnull@*/ exprNode current, 
			       exprNodeList args, 
			       int paramno, uentry ucurrent)
{
  int iparamno = 0;
  sRef thisref = exprNode_getSref (current);
  
  /*
  ** Check if any argument could match this argument.
  */
  
  exprNodeList_elements (args, icurrent) 
    {
      iparamno++;
      
      if (iparamno != paramno)
	{
	  sRef sr = exprNode_getSref (icurrent);
	  
	  if (sRef_similarRelaxed (thisref, sr))
	    {
	      if (!sRef_isConst (thisref) && !sRef_isConst (sr))
		{
		  voptgenerror 
		    (FLG_ALIASUNIQUE,
		     message
		     ("Parameter %d (%s) to function %s is declared %s but "
		      "is aliased by parameter %d (%s)",
		      paramno, 
		      exprNode_unparse (current),
		      exprNode_unparse (fcn),
		      alkind_unparse (uentry_getAliasKind (ucurrent)),
		      iparamno, exprNode_unparse (icurrent)),
		     current->loc);
		}
	    }
	  else
	    {
	      sRefSet aliases = usymtab_canAlias (sr);

	      sRefSet_allElements (aliases, asr)
		{
		  if (ctype_isUnknown (sRef_getType (thisref)))
		    {
		      sRef_setType (thisref, uentry_getType (ucurrent));
		    }
		  
		  if (sRef_similarRelaxed (thisref, asr)) 
		    {
		      if (sRef_isExternal (asr))  
			{
			  if (sRef_isLocalState (thisref))
			    {
			      ; /* okay */
			    }
			  else
			    {
			      sRef base = sRef_getRootBase (asr);
			      
			      if (!sRef_similar (sRef_getBase (asr), thisref)) 
				{
				  if (sRef_isUnique (base) || sRef_isOnly (base)
				      || sRef_isKept (base)
				      || (sRef_isAddress (asr) && sRef_isLocalVar (base))
				      || (sRef_isAddress (thisref) 
					  && sRef_isLocalVar (sRef_getRootBase (thisref))))
				    {
				      ; /* okay, no error */
				    }
				  else
				    {
				      voptgenerror 
					(FLG_MAYALIASUNIQUE,
					 message
					 ("Parameter %d (%s) to function %s is declared %s but "
					  "may be aliased externally by parameter %d (%s)",
					  paramno, 
					  exprNode_unparse (current),
					  exprNode_unparse (fcn),
					  alkind_unparse (uentry_getAliasKind (ucurrent)),
					  iparamno, exprNode_unparse (icurrent)),
					 current->loc);
				    }
				}
			    }
			}
		      else
			{
			  voptgenerror 
			    (FLG_ALIASUNIQUE,
			     message
			     ("Parameter %d (%s) to function %s is declared %s but "
			      "is aliased externally by parameter %d (%s) through "
			      "alias %q",
			      paramno, 
			      exprNode_unparse (current),
			      exprNode_unparse (fcn),
			      alkind_unparse (uentry_getAliasKind (ucurrent)),
			      iparamno, exprNode_unparse (icurrent),
			      sRef_unparse (asr)),
			     current->loc);
			}
		    }
		} end_sRefSet_allElements;
	      sRefSet_free (aliases);
	    }
	}
    } end_exprNodeList_elements;
}

