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
** cstring.c
*/

# include "lclintMacros.nf"
# include "basic.h"
# include "osd.h"
# include "portab.h"

static /*@only@*/ /*@notnull@*/ 
cstring cstring_newEmpty (void)
{
  return (cstring_create (0));
}

char cstring_firstChar (cstring s) 
{
  llassert (cstring_isDefined (s));
  llassert (cstring_length (s) > 0);

  return (s[0]);
}

char cstring_getChar (cstring s, int n) 
{
  int length = cstring_length (s);

  llassert (cstring_isDefined (s));
  llassert (n >= 1 && n <= length);

  return (s[n - 1]);
}

cstring cstring_suffix (cstring s, int n) 
{
  llassert (cstring_isDefined (s));
  llassert (n <= cstring_length (s));

  return (s + n);
}

cstring cstring_prefix (cstring s, int n) 
{
  cstring t;
  char c;
  llassert (cstring_isDefined (s));
  llassert (n <= cstring_length (s));

  c = *(s + n);
  /*@-mods@*/  /* The modifications cancel out. */
  *(s + n) = '\0';
  t = cstring_copy (s);
  *(s + n) = c;
  /*@=mods@*/

  return t;
}

/* effects If s = [0-9]*, returns s as an int.
**         else returns -1.
*/

int cstring_toPosInt (cstring s)
{
  int val = 0;

  cstring_chars (s, c)
    {
      if (isdigit ((unsigned char) c))
	{
	  val = (val * 10) + (int)(c - '0');
	}
      else
	{
	  return -1;
	}
    } end_cstring_chars ; 

  return val;
}

cstring cstring_beforeChar (cstring s, char c)
{
  if (cstring_isDefined (s))
    {
      char *cp = strchr (s, c);

      if (cp != NULL)
	{
	  cstring ret;

	  /*@-mods@*/
	  *cp = '\0';
	  ret = cstring_copy (s);
	  *cp = c;
	  /*@=mods@*/ /* modification is undone */
	  
	  return ret;
	}
    }

  return cstring_undefined;
}

void cstring_setChar (cstring s, int n, char c) 
{
  llassert (cstring_isDefined (s));
  llassert (n > 0 && n <= cstring_length (s));

  s[n - 1] = c;
}

char cstring_lastChar (cstring s) 
{
  int length;

  llassert (cstring_isDefined (s));

  length = cstring_length (s);
  llassert (length > 0);

  return (s[length - 1]);
}

/*@only@*/ cstring cstring_copy (cstring s) 
{
  if (cstring_isDefined (s))
    {
      return (mstring_copy (s));
    }
  else
    {
      return cstring_undefined;
    }
}

/*@only@*/ cstring cstring_copyLength (char *s, int len)
{
  char *res = mstring_create (len + 1);

  strncpy (res, s, size_fromInt (len));
  res[len] = '\0';
  return res;
}

bool cstring_containsChar (cstring c, char ch)
{
  if (cstring_isDefined (c))
    {
      return (strchr (c, ch) != NULL);
    }
  else
    {
      return FALSE;
    }
}

/*
** Replaces all occurances of old in s with new.
*/

# ifdef WIN32
void cstring_replaceAll (cstring s, char old, char snew)
{
  
  llassert (old != snew);

  if (cstring_isDefined (s))
    {
      char *sp = strchr (s, old);

      while (sp != NULL)
	{
	  *sp = snew;
	  sp = strchr (sp, old);
	}

          }
}
# endif

void cstring_replaceLit (/*@unique@*/ cstring s, char *old, char *snew)
{
  
  llassert (strlen (old) >= strlen (snew));

  if (cstring_isDefined (s))
    {
      char *sp = strstr (s, old);

      while (sp != NULL)
	{
	  int lendiff = size_toInt (strlen (old) - strlen (snew));
	  char *tsnew = snew;
	  
	  while (*tsnew != '\0')
	    {
	      *sp++ = *tsnew++;
	    }

	  if (lendiff > 0)
	    {
	      while (*(sp + lendiff) != '\0')
		{
		  *sp = *(sp + lendiff);
		  sp++;
		}

	      *sp = '\0';
	    }

	  sp = strstr (s, old);
	}
          }
}

/*
** removes all chars in clist from s
*/

void cstring_stripChars (cstring s, const char *clist)
{
  if (cstring_isDefined (s))
    {
      int i;
      int size = cstring_length (s);

      for (i = 0; i < size; i++)
	{
	  char c = s[i];
	  
	  if (strchr (clist, c) != NULL)
	    {
	      /* strip this char */
	      int j;
	      
	      size--;

	      for (j = i; j < size; j++)
		{
		  s[j] = s[j+1];
		}

	      s[size] = '\0'; 
	      i--;
	    }
	}
    }
}

bool cstring_contains (/*@unique@*/ cstring c, cstring sub)
{
  if (cstring_isDefined (c))
    {
      llassert (cstring_isDefined (sub));
      
      return (strstr (c, sub) != NULL);
    }
  else
    {
      return FALSE;
    }
}

static char lookLike (char c) /*@*/
{
  if (c == 'I' || c == 'l')
    {
      return '1';
    }
  else if (c == 'O' || c == 'o')
    {
      return '0';
    }
  else if (c == 'Z')
    {
      return '2';
    }
  else if (c == 'S')
    {
      return '5';
    }
  else
    {
      return c;
    }
}

cmpcode cstring_genericEqual (cstring s, cstring t,
			      int nchars,
			      bool caseinsensitive,
			      bool lookalike)
{
  if (s == t) return CGE_SAME;
  else if (cstring_isUndefined (s))
    {
      return cstring_isEmpty (t) ? CGE_SAME : CGE_DISTINCT;
    }
  else if (cstring_isUndefined (t))
    {
      return cstring_isEmpty (s) ? CGE_SAME : CGE_DISTINCT;
    }
  else
    {
      int i = 0;
      bool diffcase = FALSE;
      bool difflookalike = FALSE;

      while (*s != '\0')
	{
	  if (nchars > 0 && i >= nchars)
	    {
	      break;
	    }

	  if (*t == *s)
	    {
	      ; /* no difference */
	    }
	  else if (caseinsensitive 
		   && (toupper ((int) *t) == toupper ((int) *s)))
	    {
	      diffcase = TRUE;
	    }
	  else if (lookalike && (lookLike (*t) == lookLike (*s)))
	    {
	      difflookalike = TRUE;
	    }
	  else 
	    {
	      return CGE_DISTINCT;
	    }
	  i++;
	  s++;
	  t++;
	}

      if (*s == '\0' && *t != '\0')
	{
	  return CGE_DISTINCT;
	}

      if (diffcase)
	{
	  return CGE_CASE;
	}
      else if (difflookalike)
	{
	  return CGE_LOOKALIKE;
	}
      else
	{
	  return CGE_SAME;
	}
    }
}



bool cstring_equalFree (/*@only@*/ cstring c1, /*@only@*/ cstring c2)
{
  bool res = cstring_equal (c1, c2);
  cstring_free (c1);
  cstring_free (c2);
  return res;
}

bool cstring_equal (cstring c1, cstring c2)
{
  if (c1 == c2) return TRUE;
  else if (cstring_isUndefined (c1)) return cstring_isEmpty (c2);
  else if (cstring_isUndefined (c2)) return cstring_isEmpty (c1);
  else return (strcmp (c1, c2) == 0);
}

bool cstring_equalLen (cstring c1, cstring c2, int len)
{
  if (c1 == c2) return TRUE;
  else if (cstring_isUndefined (c1)) return cstring_isEmpty (c2);
  else if (cstring_isUndefined (c2)) return cstring_isEmpty (c1);
  else return (strncmp (c1, c2, size_fromInt (len)) == 0);
}

bool cstring_equalCaseInsensitive (cstring c1, cstring c2)
{
  if (c1 == c2) return TRUE;
  else if (cstring_isUndefined (c1)) return cstring_isEmpty (c2);
  else if (cstring_isUndefined (c2)) return cstring_isEmpty (c1);
  else return (cstring_genericEqual (c1, c2, 0, TRUE, FALSE) != CGE_DISTINCT);
}

bool cstring_equalLenCaseInsensitive (cstring c1, cstring c2, int len)
{
  llassert (len >= 0);

  if (c1 == c2) return TRUE;
  else if (cstring_isUndefined (c1)) return cstring_isEmpty (c2);
  else if (cstring_isUndefined (c2)) return cstring_isEmpty (c1);
  else return (cstring_genericEqual (c1, c2, len, TRUE, FALSE) != CGE_DISTINCT);
}

bool cstring_equalPrefix (cstring c1, char *c2)
{
  llassert (c2 != NULL);

  if (cstring_isUndefined (c1)) 
    {
      return (strlen (c2) == 0);
    }

  return (strncmp (c1, c2, strlen (c2)) == 0);
}

bool cstring_equalCanonicalPrefix (cstring c1, char *c2)
{
  llassert (c2 != NULL);

  if (cstring_isUndefined (c1)) 
    {
      return (strlen (c2) == 0);
    }

# ifdef WIN32
  /*
  ** If one has a drive specification, but the other doesn't, skip it.
  */
  
  if (strchr (c1, ':') == NULL
      && strchr (c2, ':') != NULL)
    {
      c2 = strchr (c2 + 1, ':');
    }
  else 
    {
      if (strchr (c2, ':') == NULL
	  && strchr (c1, ':') != NULL)
	{
	  c1 = strchr (c1 + 1, ':');
	}
    }

  {
    int len = size_toInt (strlen (c2));
    int i = 0;
    int slen = 0;

    if (cstring_length (c1) < len)
      {
	return FALSE;
      }

    for (i = 0; i < len; i++)
      {
	if (c1[slen] == c2[i]
	    || (osd_isConnectChar (c1[slen]) && osd_isConnectChar (c2[i])))
	  {
	    ;
	  }
	else 
	  {
	    /*
	    ** We allow \\ to match \ because MS-DOS screws up the directory
	    ** names.
	    */
	    
	    if (c1[slen] == '\\'
		&& (slen > 0
		    && c1[slen - 1] == '\\'
		    && c2[i - 1] == '\\'))
	      {
		slen++;
		if (c1[slen] != c2[i])
		  {
		    return FALSE;
		  }
	      }
	    else
	      {
		return FALSE;
	      }
	  }

	slen++;
	if (slen >= cstring_length (c1))
	  {
	    return FALSE;
	  }
      }
  }

  return TRUE;
# else
  return (strncmp (c1, c2, strlen (c2)) == 0);
# endif
}

int cstring_xcompare (cstring *c1, cstring *c2)
{
  return (cstring_compare (*c1, *c2));
}

int cstring_compare (cstring c1, cstring c2)
{
  int res;

  if (c1 == c2)
    {
      res = 0;
    }
  else if (cstring_isUndefined (c1))
    {
      if (cstring_isEmpty (c2))
	{
	  res = 0;
	}
      else
	{
	  res = 1;
	}
    }
  else if (cstring_isUndefined (c2))
    {
      if (cstring_isEmpty (c1))
	{
	  res = 0;
	}
      else
	{
	  res = -1;
	}
    }
  else
    {
      res = strcmp (c1, c2);
    }

    return (res);
}

void cstring_markOwned (/*@owned@*/ cstring s)
{
  sfreeEventually (s);
}

void cstring_free (/*@only@*/ cstring s)
{
  if (cstring_isDefined (s)) 
    {
      sfree (s);
    }
}

cstring cstring_fromChars (/*@exposed@*/ const char *cp)
{
  return (cstring) cp;
}

/*@exposed@*/ char *cstring_toCharsSafe (cstring s)
{
  static /*@only@*/ cstring emptystring = cstring_undefined;

  if (cstring_isDefined (s))
    {
      return (char *) s;
    }
  else
    {
      if (cstring_isUndefined (emptystring))
	{
	  emptystring = cstring_newEmpty ();
	}

      return emptystring;
    }
}

int cstring_length (cstring s)
{
  if (cstring_isDefined (s))
    {
      return size_toInt (strlen (s));
    }
  return 0;
}

cstring
cstring_capitalize (cstring s)
{
  if (!cstring_isEmpty (s))
    {
      cstring ret = cstring_copy (s);

      cstring_setChar (ret, 1, (char) toupper ((int) cstring_firstChar (ret)));
      return ret;
    }
  
  return cstring_undefined;
}

cstring
cstring_capitalizeFree (cstring s)
{
  if (!cstring_isEmpty (s))
    {
      cstring_setChar (s, 1, (char) toupper ((int) cstring_firstChar (s)));
      return s;
    }
  
  return s;
}

cstring
cstring_clip (cstring s, int len)
{
  if (cstring_isUndefined (s) || cstring_length (s) <= len)
    {
      ;
    }
  else
    {
      llassert (s != NULL);
      *(s + len) = '\0';
    }

  return s;
}

/*@only@*/ cstring
cstring_elide (cstring s, int len)
{
  if (cstring_isUndefined (s) || cstring_length (s) <= len)
    {
      return cstring_copy (s);
    }
  else
    {
      cstring sc = cstring_create (len);
     
      strncpy (sc, s, size_fromInt (len));
      *(sc + len - 1) = '\0';
      *(sc + len - 2) = '.';      
      *(sc + len - 3) = '.';      
      *(sc + len - 4) = '.';      
      return sc;
    }
}

/*@only@*/ cstring
cstring_fill (cstring s, int n)
{
  cstring t = cstring_create (n + 1);
  cstring ot = t;
  int len = cstring_length (s);
  int i;
  
  if (len > n)
    {
      for (i = 0; i < n; i++)
	{
	  *t++ = *s++;
	}
      *t = '\0';
    }
  else
    {
      for (i = 0; i < len; i++)
	{
	  *t++ = *s++;
	}
      for (i = 0; i < n - len; i++)
	{
	  *t++ = ' ';
	}
      *t = '\0';
    }

  return ot;
}

cstring
cstring_downcase (cstring s)
{
  if (cstring_isDefined (s))
    {
      cstring t = cstring_create (strlen (s) + 1);
      cstring ot = t;
      char c;
      
      while ((c = *s) != '\0')
	{
	  if (c >= 'A' && c <= 'Z')
	    {
	      c = c - 'A' + 'a';
	    }
	  *t++ = c;
	  s++;
	}
      *t = '\0';
      
      return ot;
    }
  else
    {
      return cstring_undefined;
    }
}

/*@notnull@*/ cstring 
cstring_appendChar (/*@only@*/ cstring s1, char c)
{
  int l = cstring_length (s1);
  char *s;

  s = (char *) dmalloc (sizeof (*s) * (l + 2));

  if (cstring_isDefined (s1))
    {  
      strcpy (s, s1);
      *(s + l) = c;
      *(s + l + 1) = '\0';
      sfree (s1); 
    }
  else
    {
      *(s) = c;
      *(s + 1) = '\0';
    } 

  return s;
}

/*@only@*/ cstring 
cstring_concatFree (cstring s, cstring t)
{
  cstring res = cstring_concat (s, t);
  cstring_free (s);
  cstring_free (t);
  return res;
}

/*@only@*/ cstring 
cstring_concatFree1 (cstring s, cstring t)
{
  cstring res = cstring_concat (s, t);
  cstring_free (s);
  return res;
}

# ifndef NOLCL
/*@only@*/ cstring 
cstring_concatChars (cstring s, char *t)
{
  cstring res = cstring_concat (s, cstring_fromChars (t));
  cstring_free (s);
  return res;
}
# endif

/*@only@*/ cstring 
cstring_concatLength (cstring s1, char *s2, int len)
{
  cstring tmp = cstring_copyLength (s2, len);
  cstring res = cstring_concat (s1, tmp);
  cstring_free (tmp);
  cstring_free (s1);

  return res;
}

/*@only@*/ cstring 
cstring_concat (cstring s, cstring t)
{
  char *ret = mstring_create (cstring_length (s) + cstring_length (t));

  if (cstring_isDefined (s))
    {
      strcpy (ret, s);
    }
  if (cstring_isDefined (t))
    {
      strcat (ret, t);
    }

  return ret;
}

/*@notnull@*/ /*@only@*/ cstring 
cstring_prependCharO (char c, /*@only@*/ cstring s1)
{
  cstring res = cstring_prependChar (c, s1);

  cstring_free (s1);
  return (res);
}

/*@notnull@*/ /*@only@*/ cstring 
cstring_prependChar (char c, /*@temp@*/ cstring s1)
{
  int l = cstring_length (s1);
  char *s = (char *) dmalloc (sizeof (*s) * (l + 2));
  
  *(s) = c;

  if (cstring_isDefined (s1)) 
    {
      /*@-mayaliasunique@*/ 
      strcpy (s + 1, s1);
      /*@=mayaliasunique@*/ 
    }

  *(s + l + 1) = '\0';
  return s;
}

# ifndef NOLCL
bool
cstring_hasNonAlphaNumBar (cstring s)
{
  int c;

  if (cstring_isUndefined (s)) return FALSE;

  while ((c = (int) *s) != (int) '\0')
    {
      if ((isalnum (c) == 0) && (c != (int) '_')
	  && (c != (int) '.') && (c != (int) CONNECTCHAR))
	{
	  return TRUE;
	}

      s++;
    }
  return FALSE;
}
# endif

/*@only@*/ /*@notnull@*/ cstring 
cstring_create (int n)
{
  char *s = dmalloc (sizeof (*s) * (n + 1));

  *s = '\0';
  return s;
}

# ifndef NOLCL
lsymbol cstring_toSymbol (cstring s)
{
  lsymbol res = lsymbol_fromChars (cstring_toCharsSafe (s));

  cstring_free (s);
  return res;
}
# endif

cstring cstring_bsearch (cstring key, char **table, int nentries)
{
  if (cstring_isDefined (key))
    {
      int low = 0;
      int high = nentries;
      int mid = (high + low + 1) / 2;
      int last = -1;
      cstring res = cstring_undefined;

      while (low <= high && mid < nentries)
	{
	  int cmp;

	  llassert (mid != last);
	  llassert (mid >= 0 && mid < nentries);

	  cmp = cstring_compare (key, table[mid]);
	  
	  if (cmp == 0)
	    {
	      res = table[mid];
	      break;
	    }
	  else if (cmp < 0) /* key is before table[mid] */
	    {
	      high = mid - 1;
	    }
	  else /* key of after table[mid] */
	    {
	      low = mid + 1;
	    }

	  last = mid;
	  mid = (high + low + 1) / 2;
	}

      if (mid != 0 && mid < nentries - 1)
	{
	  llassert (cstring_compare (key, table[mid - 1]) > 0);
	  llassert (cstring_compare (key, table[mid + 1]) < 0);
	}

      return res;
    }
  
  return cstring_undefined;
}

extern /*@observer@*/ cstring cstring_advanceWhiteSpace (cstring s)
{
  if (cstring_isDefined (s)) {
    char *t = s;

    while (*t != '\0' && isspace ((int) *t)) {
      t++;
    }

    return t;
  }
  
  return cstring_undefined;
}
    