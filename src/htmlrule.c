#include "htmlrule.h"

HTMLObject *
html_rule_new (gint max_width, gint percent, gint size, gboolean shade)
{
	HTMLRule *rule = g_new0 (HTMLRule, 1);
	HTMLObject *object = HTML_OBJECT (rule);
	html_object_init (object, Rule);

	/* HTMLObject functions */
	object->draw = html_rule_draw;
	object->set_max_width = html_rule_set_max_width;
	object->calc_min_width = html_rule_calc_min_width;

	if (size < 1)
		size = 1;
	object->ascent = 6 + size;
	object->descent = 6;
	object->width = max_width;
	object->percent = percent;

	rule->shade = shade;

	if (percent > 0) {
		object->width = max_width * percent / 100;
		object->flags &= ~FixedWidth;
	}

	return object;
}

void
html_rule_set_max_width (HTMLObject *o, gint max_width)
{
	if (!(o->flags & FixedWidth)) {
		o->max_width = max_width;
		
		if (o->percent > 0)
			o->width = max_width * o->percent / 100;
		else
			o->width = o->max_width;
	}
}

gint
html_rule_calc_min_width (HTMLObject *o)
{
	if (o->flags & FixedWidth)
		return o->width;
	
	return 1;
}

void
html_rule_draw (HTMLObject *o, HTMLPainter *p,
		gint x, gint y, gint width, gint height, gint tx, gint ty)
{
	HTMLRule *rule = HTML_RULE (o);
	gint xp, yp;

	if (y + height < o->y + o->ascent || y > o->y + o->descent)
		return;
	
	xp = o->x + tx;
	yp = o->y + ty;
	
	if (rule->shade) {
		html_painter_draw_shade_line (p, xp, yp, o->width);

	}
	else 
		html_painter_fill_rect (p, xp, yp, o->width, o->ascent - 6);
}
