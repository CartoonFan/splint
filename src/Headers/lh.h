/*
** Copyright (C) University of Virginia, Massachusetts Institue of Technology 1994-2000.
** See ../LICENSE for license information.
**
*/
/*
** lh.h
*/

extern void lhCleanup (void) /*@modifies internalState, fileSystem@*/ ;
extern void lhIncludeBool (void) /*@modifies internalState@*/ ;
extern void lhInit (tsource *p_f) /*@modifies internalState, fileSystem@*/ ;
extern void lhOutLine (/*@only@*/ cstring p_s) /*@modifies internalState@*/ ;
extern void lhExternals (interfaceNodeList p_x) /*@modifies internalState@*/ ;

extern  cstring 
  lhVarDecl (lclTypeSpecNode p_lclTypeSpec, initDeclNodeList p_initDecls, 
	     qualifierKind p_qualifier); 
extern cstring lhType (typeNode) ;
extern cstring 
  lhFunction (lclTypeSpecNode p_lclTypeSpec, declaratorNode p_declarator);
extern void lhForwardStruct (ltoken p_t) /*@modifies internalState@*/ ;
extern void lhForwardUnion (ltoken p_t) /*@modifies internalState@*/ ;