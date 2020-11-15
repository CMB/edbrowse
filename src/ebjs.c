/* ebjs.c: edbrowse javascript engine interface.
 *
 * Launch the js engine process and communicate with it to build
 * js objects and run js code.
 * Also provide some wrapper functions like get_property_string,
 * so that edbrowse can call functions to manipulate js objects,
 * thus hiding the details of sending messages to the js process
 * and receiving replies from same. */

#include "eb.h"

#include <stdarg.h>

/* Javascript has changed an input field */
static void javaSetsInner(jsobjtype v, const char *newtext);
void javaSetsTagVar(jsobjtype v, const char *newtext)
{
	Tag *t = tagFromJavaVar(v);
	if (!t)
		return;
	if (t->itype == INP_HIDDEN || t->itype == INP_RADIO
	    || t->itype == INP_FILE)
		return;
	if (t->itype == INP_TA) {
		javaSetsInner(v, newtext);
		return;
	}
	nzFree(t->value);
	t->value = cloneString(newtext);
}				/* javaSetsTagVar */

static void javaSetsInner(jsobjtype v, const char *newtext)
{
	int side;
	Tag *t = tagFromJavaVar(v);
	if (!t)
		return;
/* the tag should always be a textarea tag. */
	if (t->action != TAGACT_INPUT || t->itype != INP_TA) {
		debugPrint(3,
			   "innerText is applied to tag %d that is not a textarea.",
			   t->seqno);
		return;
	}
	side = t->lic;
	if (side <= 0 || side >= MAXSESSION || side == context)
		return;
	if (sessionList[side].lw == NULL)
		return;
	if (cw->browseMode)
		i_printf(MSG_BufferUpdated, side);
	sideBuffer(side, newtext, -1, 0);
}				/* javaSetsInner */

/*********************************************************************
See if a tag object is still rooted on the js side.
If not, it could be garbage collected away. We could be accessing a bad pointer.
Worse, it could be reallocated to a new object,
so we're not even accessing the object we think we are.
Imagine a timer fires and js rearranges the entire tree, but we haven't
rerendered yet. You type g on a link.
That line isn't even there, the tag is obsolete, its pointer is obsolete.
Check for that here in the only way I think is safe, from the top.
However, if objects are rooted in some way,
so that they can't go away without being released,
then there is an easier way. Start with the given node and climb up
through parentNode, looking for a global object.
*********************************************************************/

bool tagIsRooted(Tag *t)
{
Tag *u, *v = 0, *w;

	for(u = t; u; v = u, u = u->parent) {
		u->lic = -1;
		if(!v)
			continue;
		for(w = u->firstchild; w; w = w->sibling) {
			++u->lic;
			if(w == v)
				break;
		}
		if(!w) // this should never happen!
			goto fail;
// lic is the count of the child in the chain
	}
	u = v;

/*********************************************************************
We're at the top. Should be html.
There's no other <html> tag, even if the page has subframes,
so this should be a rock solid test.
*********************************************************************/

	if(u->action != TAGACT_HTML)
		goto fail;

// Now climb down the chain from u to t.
// I don't know why we would ever click on or even examine a tag under <head>,
// but I guess I'll allow for the possibility.
	if(u->lic == 0) // head
		u = u->firstchild;
	else if(u->lic == 1) // body
		u = u->firstchild->sibling;
	else // should never happen
		goto fail;

	while(true) {
		int i, len;
		jsobjtype cn; // child nodes
// Imagine removing an object from the tree, allocating a new one, and by sheer
// bad luck, the new object gets the same pointer. Then put it back in the
// same place in the tree. I've seen it happen.
// Use our sseqno to defend against this.
		if(get_property_number_0(u->f0->cx, u->jv, "eb$seqno") != u->seqno)
			goto fail;
		if(u == t)
			break;
		i = 0;
		v = u->firstchild;
		while(++i <= u->lic)
			v = v->sibling;
		if(!v->jv)
			goto fail;
// find v->jv in the children of u.
		if(!(cn = get_property_object_0(u->f0->cx, u->jv, "childNodes")))
			goto fail;
		len = get_arraylength_0(u->f0->cx, cn);
		for(i = 0; i < len; ++i)
			if(get_array_element_object_0(u->f0->cx, cn, i) == v->jv) // found it
				break;
		if(i == len)
			goto fail; // not found
		u = v;
	}

	debugPrint(4, "%s %d is rooted", t->info->name, t->seqno);
	return true; // properly rooted

fail:
	debugPrint(3, "%s %d is not rooted", t->info->name, t->seqno);
	return false;
}

/*********************************************************************
Javascript sometimes builds or rebuilds a submenu, based upon your selection
in a primary menu. These new options must map back to html tags,
and then to the dropdown list as you interact with the form.
This is tested in jsrt - select a state,
whereupon the colors below, that you have to choose from, can change.
This does not easily fold into rerender(),
since the line <> in the buffer looks exactly the same,
so this tells you the options underneath have changed.
*********************************************************************/

static void rebuildSelector(Tag *sel, jsobjtype oa, int len2)
{
	int i2 = 0;
	bool check2;
	char *s;
	const char *selname;
	bool changed = false;
	Tag *t, *t0 = 0;
	jsobjtype oo;		/* option object */
	jsobjtype cx = sel->f0->cx;

	selname = sel->name;
	if (!selname)
		selname = "?";
	debugPrint(4, "testing selector %s %d", selname, len2);
	sel->lic = (sel->multiple ? 0 : -1);
	t = cw->optlist;

	while (t && i2 < len2) {
		t0 = t;
/* there is more to both lists */
		if (t->controller != sel) {
			t = t->same;
			continue;
		}

/* find the corresponding option object */
		if ((oo = get_array_element_object_0(cx, oa, i2)) == NULL) {
/* Wow this shouldn't happen. */
/* Guess I'll just pretend the array stops here. */
			len2 = i2;
			break;
		}

		if (t->jv != oo) {
			debugPrint(5, "oo switch");
/*********************************************************************
Ok, we freed up the old options, and garbage collection
could well kill the tags that went with these options,
i.e. the tags we're looking at now.
I'm bringing the tags back to life.
*********************************************************************/
			t->dead = false;
			disconnectTagObject(t);
			connectTagObject(t, oo);
		}

		t->rchecked = get_property_bool_0(cx, oo, "defaultSelected");
		check2 = get_property_bool_0(cx, oo, "selected");
		if (check2) {
			if (sel->multiple)
				++sel->lic;
			else
				sel->lic = i2;
		}
		++i2;
		if (t->checked != check2)
			changed = true;
		t->checked = check2;
		s = get_property_string_0(cx, oo, "text");
		if ((s && !t->textval) || !stringEqual(t->textval, s)) {
			nzFree(t->textval);
			t->textval = s;
			changed = true;
		} else
			nzFree(s);
		s = get_property_string_0(cx, oo, "value");
		if ((s && !t->value) || !stringEqual(t->value, s)) {
			nzFree(t->value);
			t->value = s;
		} else
			nzFree(s);
		t = t->same;
	}

/* one list or the other or both has run to the end */
	if (i2 == len2) {
		for (; t; t = t->same) {
			if (t->controller != sel) {
				t0 = t;
				continue;
			}
/* option is gone in js, disconnect this option tag from its select */
			disconnectTagObject(t);
			t->controller = 0;
			t->action = TAGACT_NOP;
			if (t0)
				t0->same = t->same;
			else
				cw->optlist = t->same;
			changed = true;
		}
	} else if (!t) {
		for (; i2 < len2; ++i2) {
			if ((oo = get_array_element_object_0(cx, oa, i2)) == NULL)
				break;
			t = newTag(sel->f0, "option");
			t->lic = i2;
			t->controller = sel;
			connectTagObject(t, oo);
			t->step = 2;	// already decorated
			t->textval = get_property_string_0(cx, oo, "text");
			t->value = get_property_string_0(cx, oo, "value");
			t->checked = get_property_bool_0(cx, oo, "selected");
			if (t->checked) {
				if (sel->multiple)
					++sel->lic;
				else
					sel->lic = i2;
			}
			t->rchecked = get_property_bool_0(cx, oo, "defaultSelected");
			changed = true;
		}
	}

	if (!changed)
		return;
	debugPrint(4, "selector %s has changed", selname);

	s = displayOptions(sel);
	if (!s)
		s = emptyString;
	javaSetsTagVar(sel->jv, s);
	nzFree(s);

	if (!sel->multiple)
		set_property_number_0(cx, sel->jv, "selectedIndex", sel->lic);
}				/* rebuildSelector */

void rebuildSelectors(void)
{
	int i1;
	Tag *t;
	jsobjtype oa;		/* option array */
	int len;		/* length of option array */

	if (!isJSAlive)
		return;

	for (i1 = 0; i1 < cw->numTags; ++i1) {
		t = tagList[i1];
		if (!t->jv)
			continue;
		if (t->action != TAGACT_INPUT)
			continue;
		if (t->itype != INP_SELECT)
			continue;
#if 0
		if(!tagIsRooted(t))
			continue;
#endif

/* there should always be an options array, if not then move on */
		if ((oa = get_property_object_0(t->f0->cx, t->jv, "options")) == NULL)
			continue;
		if ((len = get_arraylength_0(t->f0->cx, oa)) < 0)
			continue;
		rebuildSelector(t, oa, len);
	}

}				/* rebuildSelectors */
