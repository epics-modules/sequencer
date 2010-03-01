/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1989-93, The Regents of the University of California.
		         Los Alamos National Laboratory

 	parse.h,v 1.2 1995/06/27 15:25:50 wright Exp
	DESCRIPTION: Structures for parsing the state notation language.
	ENVIRONMENT: UNIX
	HISTORY:
18nov91,ajk	Replaced lstLib stuff with in-line links.
28oct93,ajk	Added support for assigning array elements to pv's.
28oct93,ajk	Added support for pointer declarations (see VC_*)
05nov93,ajk	Changed structures var & db_chan to handle array assignments.
05nov93,ajk	changed malloc() to calloc() 3 places.
20jul95,ajk	Added unsigned types (V_U...).
08aug96,wfl	Added syncQ variables to var struct.
01sep99,grw     Added E_OPTION, E_ENTRY, E_EXIT.
07sep99,wfl	Added E_DECL (for local variable declarations).
***************************************************************************/

#ifndef INCLparseh
#define INCLparseh

/* Allow this file to be included after snc.h */
#ifdef INCLsnch
#undef Expr
#undef Var
#endif

/* Data for these blocks are generated by the parsing routines for each
** state set.  The tables are then used to generate the run-time C code
** for the sequencer.  This decouples the parsing implementation from
** the run-time code implementation.
*/

struct	expression			/* Expression block */
{
	struct	expression *next;	/* link to next expression */
	struct	expression *last;	/* link to last in list */
	struct	expression *left;	/* ptr to left expression */
	struct	expression *right;	/* ptr to right expression */
	int	type;			/* expression type (E_*) */
	char	*value;			/* operator or value string */
	int	line_num;		/* line number */
	char	*src_file;		/* effective source file */
};
typedef	struct	expression Expr;

struct	var				/* Variable or function definition */
{
	struct	var *next;		/* link to next item in list */
	char	*name;			/* variable name */
	char	*value;			/* initial value or NULL */
	int	type;			/* var type */
	int	class;			/* simple, array, or pointer */
	int	length1;		/* 1st dim. array lth (default=1) */
	int	length2;		/* 2nd dim. array lth (default=1) */
	int	ef_num;			/* bit number if this is an event flag */
	struct	db_chan *chan;		/* ptr to channel struct if assigned */
	int	queued;			/* whether queued via syncQ */
	int	maxQueueSize;		/* max syncQ queue size */
	int	queueIndex;		/* index in syncQ queue array */

};
typedef	struct	var Var;

struct	db_chan				/* DB channel assignment info */
{
	struct	db_chan *next;		/* link to next item in list */
	char	*db_name;		/* database name (assign all to 1 pv) */
	char	**db_name_list;		/* list of db names (assign each to a pv) */
	int	num_elem;		/* number of elements assigned in db_name_list */
	Var	*var;			/* ptr to variable definition */
	int	count;			/* count for db access */
	int	mon_flag;		/* TRUE if channel is "monitored" */
	int	*mon_flag_list;		/* ptr to list of monitor flags */
	Var	*ef_var;		/* ptr to event flag variable for sync */
	Var	**ef_var_list;		/* ptr to list of event flag variables */
	int	ef_num;			/* event flag number */
	int	*ef_num_list;		/* list of event flag numbers */
	int	index;			/* index in database channel array (seqChan) */
};
typedef	struct	db_chan Chan;
/* Note: Only one of db_name or db_name_list can have a non-zero value */

/* Linked list allocation definitions */
#define	allocExpr()		(Expr *)calloc(1, sizeof(Expr));
#define	allocVar()		(Var *)calloc(1, sizeof(Var));
#define	allocChan()		(Chan *)calloc(1, sizeof(Chan));

/* Variable types */
#define	V_NONE		0		/* not defined */
#define	V_CHAR		1		/* char */
#define	V_SHORT		2		/* short */
#define	V_INT		3		/* int */
#define	V_LONG		4		/* long */
#define	V_FLOAT		5		/* float */
#define	V_DOUBLE	6		/* double */
#define	V_STRING	7		/* strings (array of char) */
#define	V_EVFLAG	8		/* event flag */
#define	V_FUNC		9		/* function (not a variable) */
#define	V_UCHAR		11		/* unsigned char */
#define	V_USHORT	12		/* unsigned short */
#define	V_UINT		13		/* unsigned int */
#define	V_ULONG		14		/* unsigned long */

/* Variable classes */
#define	VC_SIMPLE	0		/* simple (un-dimensioned) variable */
#define	VC_ARRAY1	1		/* single dim. array */
#define	VC_ARRAY2	2		/* multiple dim. array */
#define	VC_POINTER	3		/* pointer */
#define	VC_ARRAYP	4		/* array of pointers */

/* Expression types */
#define	E_EMPTY		0		/* empty expression */
#define	E_CONST		1		/* numeric constant */
#define	E_VAR		2		/* variable */
#define	E_FUNC		3		/* function */
#define	E_STRING	4		/* ptr to string constant */
#define	E_UNOP		5		/* unary operator: OP expr (-, +, or !) */
#define	E_BINOP		6		/* binary operator: expr OP expr */
#define	E_ASGNOP	7		/* assign operator:  (=, +=, *=, etc.) */
#define	E_PAREN		8		/* parenthesis around an expression */
#define	E_SUBSCR	9		/* subscript */
#define	E_TEXT		10		/* C code or other text to be inserted */
#define	E_STMT		11		/* simple statement */
#define	E_CMPND		12		/* begin compound statement: {...} */
#define	E_IF		13		/* if statement */
#define	E_ELSE		14		/* else statement */
#define	E_WHILE		15		/* while statement */
#define	E_SS		16		/* state set statement */
#define	E_STATE		17		/* state statement */
#define	E_WHEN		18		/* when statement */
#define	E_FOR		19		/* for statement */
#define	E_X		20		/* eXpansion (e.g. for(;;) */
#define	E_PRE		21		/* ++expr or --expr */
#define	E_POST		22		/* expr++ or expr-- */
#define	E_BREAK		23		/* break stmt */
#define	E_COMMA		24		/* expr , expr */
#define E_DECL		25		/* local declaration statement */
#define E_ENTRY         26              /* entry statement */
#define E_EXIT          27              /* exit statement */
#define E_OPTION        28              /* state option statement */

void program(Expr *prog_list);
void program_name(char *pname, char *pparam);
void assign_single(
	char	*name,		/* ptr to variable name */
	char	*db_name	/* ptr to db name */
);
void assign_subscr(
	char	*name,		/* ptr to variable name */
	char	*subscript,	/* subscript value or NULL */
	char	*db_name	/* ptr to db name */
);
void assign_list(
	char	*name,		/* ptr to variable name */
	Expr	*db_name_list	/* ptr to db name list */
);
Expr *expression(
	int	type,		/* E_BINOP, E_ASGNOP, etc */
	char	*value,		/* "==", "+=", var name, constant, etc. */	
	Expr	*left,		/* LH side */
	Expr	*right		/* RH side */
);
void monitor_stmt(
	char	*name,		/* variable name (should be assigned) */
	char	*subscript	/* element number or NULL */
);
void set_debug_print(char *opt);
void decl_stmt(
	int	type,		/* variable type (e.g. V_FLOAT) */
	int	class,		/* variable class (e.g. VC_ARRAY) */
	char	*name,		/* ptr to variable name */
	char	*s_length1,	/* array lth (1st dim, arrays only) */
	char	*s_length2,	/* array lth (2nd dim, [n]x[m] arrays only) */
	char	*value		/* initial value or NULL */
);
void sync_stmt(char *name, char *subscript, char *ef_name);
void syncq_stmt(char *name, char *subscript, char *ef_name, char *maxQueueSize);
void defn_c_stmt(
	Expr *c_list	/* ptr to C code */
);
void option_stmt(
	char	*option,	/* "a", "r", ... */
	int	value		/* TRUE means +, FALSE means - */
);
int entry_code(Expr *ep);
int exit_code(Expr *ep);
void pp_code(char *line, char *fname);
void global_c_stmt(
	Expr	*c_list		/* ptr to C code */
);
Var *find_var(char *name);
Expr *link_expr(
	Expr	*ep1,	/* beginning of 1-st structure or list */
	Expr	*ep2	/* beginning 2-nd (append it to 1-st) */
);

#endif	/*INCLparseh*/
