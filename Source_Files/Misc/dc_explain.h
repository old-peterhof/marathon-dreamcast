/*
 *	dc_explain.h -- the line of plain English under a settings panel.
 *	See dc_explain.cpp.
 */

#ifndef DC_EXPLAIN_H
#define DC_EXPLAIN_H

#ifdef DC

#define EXPLAIN_MAX_LINES	3
#define EXPLAIN_MAX_CHARS	128
#define DC_EXPLAIN_MAX		40

class w_explain : public widget {
public:
	w_explain(int max_lines = 2);

	void set(const char *text);
	void draw(SDL_Surface *s) const;
	bool is_selectable(void) const { return false; }

private:
	int lines;
	char wrapped[EXPLAIN_MAX_LINES][EXPLAIN_MAX_CHARS];
};

/*
 *	Usage, per screen:
 *
 *		w_explain *line = new w_explain;
 *		dc_explain_begin(line);
 *		... d.add(row); dc_explain_add(row, "what it does"); ...
 *		d.add(line);
 *		dc_explain_arm(&d);
 *		d.run();
 *		dc_explain_end();
 */
void dc_explain_begin(w_explain *line);
void dc_explain_add(widget *w, const char *text);
void dc_explain_arm(dialog *d);
void dc_explain_end(void);

#endif	/* DC */

#endif	/* DC_EXPLAIN_H */
