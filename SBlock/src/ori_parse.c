/********************************************************************************/
/* Copyright (c) 2004                                                           */
/* Daniel Sleator, David Temperley, and John Lafferty                           */
/* All rights reserved                                                          */
/*                                                                              */
/* Use of the link grammar parsing system is subject to the terms of the        */
/* license set forth in the LICENSE file included with this software,           */ 
/* and also available at http://www.link.cs.cmu.edu/link/license.html           */
/* This license allows free redistribution and use in source and binary         */
/* forms, with or without modification, subject to certain conditions.          */
/*                                                                              */
/********************************************************************************/

 /****************************************************************************
 *  
 *   This is a simple example of the link parser API.  It similates most of
 *   the functionality of the original link grammar parser, allowing sentences
 *   to be typed in either interactively or in "batch" mode (if -batch is
 *   specified on the command line, and stdin is redirected to a file).
 *   The program:
 *     Opens up a dictionary
 *     Iterates:
 *        1. Reads from stdin to get an input string to parse
 *        2. Tokenizes the string to form a Sentence
 *        3. Tries to parse it with cost 0
 *        4. Tries to parse with increasing cost
 *     When a parse is found:
 *        1. Extracts each Linkage
 *        2. Passes it to process_some_linkages()
 *        3. Deletes linkage
 *     After parsing each Sentence is deleted by making a call to 
 *     sentence_delete.
 *     
 ****************************************************************************/


#include "link-includes.h"
#include "command-line.h"
#include "lcd_22.h"

#define MAXINPUT 1024
#define DISPLAY_MAX 1024
#define COMMENT_CHAR '%'  /* input lines beginning with this are ignored */

static int batch_errors = 0;
static int input_pending=FALSE;
static Parse_Options  opts;
static Parse_Options  panic_parse_opts;

typedef enum {UNGRAMMATICAL='*', 
	      PARSE_WITH_DISJUNCT_COST_GT_0=':',
	      NO_LABEL=' '} Label;

int fget_input_string(char *input_string, FILE *in, FILE *out, 
		      Parse_Options opts) {
    if ((!parse_options_get_batch_mode(opts)) && (verbosity > 0)
	&& (!input_pending)) fprintf(out, "linkparser> ");
    fflush(out);
    input_pending = FALSE;
    if (fgets(input_string, MAXINPUT, in)) return 1;
    else return 0;
}

int fget_input_char(FILE *in, FILE *out, Parse_Options opts) {
    if (!parse_options_get_batch_mode(opts) && (verbosity > 0)) 
      fprintf(out, "linkparser> ");
    fflush(out);
    return getc(in);
}

/**************************************************************************
*  
*  This procedure displays a linkage graphically.  Since the diagrams 
*  are passed as character strings, they need to be deleted with a 
*  call to string_delete.
*
**************************************************************************/

void process_linkage(Linkage linkage, Parse_Options opts) {
    char * string;
    int    j, mode, first_sublinkage;

    if (parse_options_get_display_union(opts)) {
	linkage_compute_union(linkage);
	first_sublinkage = linkage_get_num_sublinkages(linkage)-1;
    }
    else {
	first_sublinkage = 0;
    }

    for (j=first_sublinkage; j<linkage_get_num_sublinkages(linkage); ++j) {
	linkage_set_current_sublinkage(linkage, j);
	if (parse_options_get_display_on(opts)) {
	    string = linkage_print_diagram(linkage);
	    fprintf(stdout, "%s", string);
	    string_delete(string);
	}
	if (parse_options_get_display_links(opts)) {
	    string = linkage_print_links_and_domains(linkage);
	    fprintf(stdout, "%s", string);
	    string_delete(string);
	}
	if (parse_options_get_display_postscript(opts)) {
	    string = linkage_print_postscript(linkage, FALSE);
	    fprintf(stdout, "%s\n", string);
	    string_delete(string);
	}
    }
    if ((mode=parse_options_get_display_constituents(opts))) {
	string = linkage_print_constituent_tree(linkage, mode);
	if (string != NULL) {
	    fprintf(stdout, "%s\n", string);
	    string_delete(string);
	} else {
	    fprintf(stderr, "Can't generate constituents.\n");
	    fprintf(stderr, "Constituent processing has been turned off.\n");
	}
    }
}

void print_parse_statistics(Sentence sent, Parse_Options opts) {
    if (sentence_num_linkages_found(sent) > 0) {
	if (sentence_num_linkages_found(sent) > 
	    parse_options_get_linkage_limit(opts)) {
	    fprintf(stdout, "Found %d linkage%s (%d of %d random " \
		    "linkages had no P.P. violations)", 
		    sentence_num_linkages_found(sent), 
		    sentence_num_linkages_found(sent) == 1 ? "" : "s",
		    sentence_num_valid_linkages(sent),
		    sentence_num_linkages_post_processed(sent));
	}
	else {
	    fprintf(stdout, "Found %d linkage%s (%d had no P.P. violations)", 
		    sentence_num_linkages_post_processed(sent), 
		    sentence_num_linkages_found(sent) == 1 ? "" : "s",
		    sentence_num_valid_linkages(sent));
	}
	if (sentence_null_count(sent) > 0) {
	    fprintf(stdout, " at null count %d", sentence_null_count(sent));
	}
	fprintf(stdout, "\n");
    }
}


void process_some_linkages(Sentence sent, Parse_Options opts) {
    int i, c, num_displayed, num_to_query;
    Linkage linkage;
   
    if (verbosity > 0) print_parse_statistics(sent, opts);
    if (!parse_options_get_display_bad(opts)) {
	num_to_query = MIN(sentence_num_valid_linkages(sent), DISPLAY_MAX);
    }
    else {
	num_to_query = MIN(sentence_num_linkages_post_processed(sent), 
			   DISPLAY_MAX);
    }

    for (i=0, num_displayed=0; i<num_to_query; ++i) {

	if ((sentence_num_violations(sent, i) > 0) &&
	    (!parse_options_get_display_bad(opts))) {
	    continue;
	}

	linkage = linkage_create(i, sent, opts);

	if (verbosity > 0) {
	  if ((sentence_num_valid_linkages(sent) == 1) &&
	      (!parse_options_get_display_bad(opts))) {
	    fprintf(stdout, "  Unique linkage, ");
	  }
	  else if ((parse_options_get_display_bad(opts)) &&
		   (sentence_num_violations(sent, i) > 0)) {
	    fprintf(stdout, "  Linkage %d (bad), ", i+1);
	  }
	  else {
	    fprintf(stdout, "  Linkage %d, ", i+1);
	  }
	  
	  if (!linkage_is_canonical(linkage)) {
	    fprintf(stdout, "non-canonical, ");
	  }
	  if (linkage_is_improper(linkage)) {
	    fprintf(stdout, "improper fat linkage, ");
	  }
	  if (linkage_has_inconsistent_domains(linkage)) {
	    fprintf(stdout, "inconsistent domains, ");
	  }
	  
	  fprintf(stdout, "cost vector = (UNUSED=%d DIS=%d AND=%d LEN=%d)\n",
		  linkage_unused_word_cost(linkage),
		  linkage_disjunct_cost(linkage),
		  linkage_and_cost(linkage),
		  linkage_link_cost(linkage));
	}

	process_linkage(linkage, opts);
	linkage_delete(linkage);

	if (++num_displayed < num_to_query) {
	    if (verbosity > 0) {
	        fprintf(stdout, "Press RETURN for the next linkage.\n");
	    }
	    if ((c=fget_input_char(stdin, stdout, opts)) != '\n') {
		ungetc(c, stdin);
		input_pending = TRUE;
		break;
	    }
	}
    }
}

int there_was_an_error(Label label, Sentence sent, Parse_Options opts) {

    if (sentence_num_valid_linkages(sent) > 0) {
	if (label == UNGRAMMATICAL) {
	    batch_errors++;
	    return UNGRAMMATICAL;
	}
	if ((sentence_disjunct_cost(sent, 0) == 0) && 
	    (label == PARSE_WITH_DISJUNCT_COST_GT_0)) {
	    batch_errors++;
	    return PARSE_WITH_DISJUNCT_COST_GT_0;
	}
    } else {
	if  (label != UNGRAMMATICAL) {
	    batch_errors++;
	    return UNGRAMMATICAL;
	}
    }
    return FALSE;
}

void batch_process_some_linkages(Label label, Sentence sent, Parse_Options opts) {
    Linkage linkage;
   
    if (there_was_an_error(label, sent, opts)) {
	if (sentence_num_linkages_found(sent) > 0) {
	    linkage = linkage_create(0, sent, opts);
	    process_linkage(linkage, opts);
	    linkage_delete(linkage);
	}
	fprintf(stdout, "+++++ error %d\n", batch_errors);
    }
}

int special_command(char *input_string, Dictionary dict) {

    if (input_string[0] == '\n') return TRUE;
    if (input_string[0] == COMMENT_CHAR) return TRUE;
    if (input_string[0] == '!') {
        if (strncmp(input_string, "!panic_", 7)==0) {
	    issue_special_command(input_string+7, panic_parse_opts, dict);
	}
	else {
	    issue_special_command(input_string+1, opts, dict);
	}
	return TRUE;
    }
    return FALSE;
}

Label strip_off_label(char * input_string) {
    int c;

    c = input_string[0];
    switch(c) {
    case UNGRAMMATICAL:
    case PARSE_WITH_DISJUNCT_COST_GT_0:
	input_string[0] = ' ';
	return c;
	break;
    default:
	return NO_LABEL;
    }	
}

void setup_panic_parse_options(Parse_Options opts) {
    parse_options_set_disjunct_cost(opts, 3);
    parse_options_set_min_null_count(opts, 1);
    parse_options_set_max_null_count(opts, MAX_SENTENCE);
    parse_options_set_max_parse_time(opts, 60);
    parse_options_set_islands_ok(opts, 1);
    parse_options_set_short_length(opts, 6);
    parse_options_set_all_short_connectors(opts, 1);
    parse_options_set_linkage_limit(opts, 100);
}

void print_usage(char *str) {
    fprintf(stderr, 
	    "Usage: %s [dict_file] [-pp pp_knowledge_file]\n"
	    "          [-c constituent_knowledge_file] [-a affix_file]\n"
	    "          [-ppoff] [-coff] [-aoff] [-batch] [-<special \"!\" command>]\n", str);
    exit(-1);
}

int main(int argc, char * argv[]) {

    Dictionary      dict;
    Sentence        sent;
    char            *dictionary_file=NULL;
    char            *post_process_knowledge_file=NULL;
    char            *constituent_knowledge_file=NULL;
    char            *affix_file=NULL;
    int             pp_on=TRUE;
    int             af_on=TRUE;
    int             cons_on=TRUE;
    int             num_linkages, i;
    char            input_string[MAXINPUT];
    Label           label = NO_LABEL;  
    int             parsing_space_leaked, reported_leak, dictionary_and_option_space;


    i = 1;
    if ((argc > 1) && (argv[1][0] != '-')) {
	/* the dictionary is the first argument if it doesn't begin with "-" */
	dictionary_file = argv[1];	
	i++;
    }

    for (; i<argc; i++) {
	if (argv[i][0] == '-') {
	    if (strcmp("-pp", argv[i])==0) {
		if ((post_process_knowledge_file != NULL) || (i+1 == argc)) 
		  print_usage(argv[0]);
		post_process_knowledge_file = argv[i+1];
		i++;
	    } else 
	    if (strcmp("-c", argv[i])==0) {
		if ((constituent_knowledge_file != NULL) || (i+1 == argc)) 
		  print_usage(argv[0]);
		constituent_knowledge_file = argv[i+1];
		i++;
	    } else 
	    if (strcmp("-a", argv[i])==0) {
		if ((affix_file != NULL) || (i+1 == argc)) print_usage(argv[0]);
		affix_file = argv[i+1];
		i++;
	    } else if (strcmp("-ppoff", argv[i])==0) {
		pp_on = FALSE;
	    } else if (strcmp("-coff", argv[i])==0) {
		cons_on = FALSE;
	    } else if (strcmp("-aoff", argv[i])==0) {
		af_on = FALSE;
	    } else if (strcmp("-batch", argv[i])==0) {
	    } else if (strncmp("-!", argv[i],2)==0) {
	    } else {
		print_usage(argv[0]);		
	    }
	} else {
	    print_usage(argv[0]);
	}
    }

    if (!pp_on && post_process_knowledge_file != NULL) print_usage(argv[0]);

    if (dictionary_file == NULL) {
	dictionary_file = "4.0.dict";
        fprintf(stderr, "No dictionary file specified.  Using %s.\n", 
		dictionary_file);
    }

    if (af_on && affix_file == NULL) {
	affix_file = "4.0.affix";
        fprintf(stderr, "No affix file specified.  Using %s.\n", affix_file);
    }

    if (pp_on && post_process_knowledge_file == NULL) {
	post_process_knowledge_file = "4.0.knowledge";
        fprintf(stderr, "No post process knowledge file specified.  Using %s.\n",
		post_process_knowledge_file);
    }

    if (cons_on && constituent_knowledge_file == NULL) {
        constituent_knowledge_file = "4.0.constituent-knowledge"; 
	fprintf(stderr, "No constituent knowledge file specified.  Using %s.\n", 
		constituent_knowledge_file);
    }

    opts = parse_options_create();
    if (opts == NULL) {
	fprintf(stderr, "%s\n", lperrmsg);
	exit(-1);
    }

    panic_parse_opts = parse_options_create();
    if (panic_parse_opts == NULL) {
	fprintf(stderr, "%s\n", lperrmsg);
	exit(-1);
    }
    setup_panic_parse_options(panic_parse_opts);
    parse_options_set_max_sentence_length(opts, 70);
    parse_options_set_panic_mode(opts, TRUE);
    parse_options_set_max_parse_time(opts, 30);
    parse_options_set_linkage_limit(opts, 1000);
    parse_options_set_short_length(opts, 10);

    dict = dictionary_create(dictionary_file, 
			     post_process_knowledge_file,
			     constituent_knowledge_file,
			     affix_file);
    if (dict == NULL) {
	fprintf(stderr, "%s\n", lperrmsg);
	exit(-1);
    }

    /* process the command line like commands */
    for (i=1; i<argc; i++) {
	if ((strcmp("-pp", argv[i])==0) || 
	    (strcmp("-c", argv[i])==0) || 
	    (strcmp("-a", argv[i])==0)) {
	  i++;
	} else if ((argv[i][0] == '-') && (strcmp("-ppoff", argv[i])!=0) &&
		   (argv[i][0] == '-') && (strcmp("-coff", argv[i])!=0) &&
		   (argv[i][0] == '-') && (strcmp("-aoff", argv[i])!=0)) {
	  issue_special_command(argv[i]+1, opts, dict);
	}
    }

    dictionary_and_option_space = space_in_use;  
    reported_leak = external_space_in_use = 0;
    verbosity = parse_options_get_verbosity(opts);

    //HUNJAE 
    while (fget_input_string(input_string, stdin, stdout, opts)) {

	if (space_in_use != dictionary_and_option_space + reported_leak) {
	    fprintf(stderr, "Warning: %d bytes of space leaked.\n",
		    space_in_use-dictionary_and_option_space-reported_leak);
	    reported_leak = space_in_use - dictionary_and_option_space;
	}

	if ((strcmp(input_string, "quit\n")==0) ||
	    (strcmp(input_string, "exit\n")==0)) break;

	if (special_command(input_string, dict)) continue;
	if (parse_options_get_echo_on(opts)) {
	    printf("%s", input_string);
	}

	if (parse_options_get_batch_mode(opts)) {
	    label = strip_off_label(input_string);
	}

	sent = sentence_create(input_string, dict);

	if (sent == NULL) {
	    if (verbosity > 0) fprintf(stderr, "%s\n", lperrmsg);
	    if (lperrno != NOTINDICT) exit(-1);
	    else continue;
	} 
	if (sentence_length(sent) > parse_options_get_max_sentence_length(opts)) {
	    sentence_delete(sent);
	    if (verbosity > 0) {
	      fprintf(stdout, 
		      "Sentence length (%d words) exceeds maximum allowable (%d words)\n",
		    sentence_length(sent), parse_options_get_max_sentence_length(opts));
	    }
	    continue;
	}

	/* First parse with cost 0 or 1 and no null links */
	parse_options_set_disjunct_cost(opts, 2);
	parse_options_set_min_null_count(opts, 0);
	parse_options_set_max_null_count(opts, 0);
	parse_options_reset_resources(opts);

	num_linkages = sentence_parse(sent, opts);

	/* Now parse with null links */
	if ((num_linkages == 0) && (!parse_options_get_batch_mode(opts))) {
	    if (verbosity > 0){ 
		//HUNJAE (Wrong grammer)
		fprintf(stdout, "Wrong!\n");
		
		//Call LCD Function
		start_lcd(0);
	    }
	    if (parse_options_get_allow_null(opts)) {
		parse_options_set_min_null_count(opts, 1);
		parse_options_set_max_null_count(opts, sentence_length(sent));
		num_linkages = sentence_parse(sent, opts);
	    }
	}else{
		//HUNJAE (Correct grammer)
		fprintf(stdout, "Correct!\n");
		
		//Call LCD Function
		start_lcd(1);
	}

	if (parse_options_timer_expired(opts)) {
	    if (verbosity > 0) fprintf(stdout, "Timer is expired!\n");
	}
	if (parse_options_memory_exhausted(opts)) {
	    if (verbosity > 0) fprintf(stdout, "Memory is exhausted!\n");
	}

	if ((num_linkages == 0) && 
	    parse_options_resources_exhausted(opts) &&
	    parse_options_get_panic_mode(opts)) {
	    print_total_time(opts);
	    if (verbosity > 0) fprintf(stdout, "Entering \"panic\" mode...\n");
	    parse_options_reset_resources(panic_parse_opts);
	    parse_options_set_verbosity(panic_parse_opts, verbosity);
	    num_linkages = sentence_parse(sent, panic_parse_opts);
	    if (parse_options_timer_expired(panic_parse_opts)) {
		if (verbosity > 0) fprintf(stdout, "Timer is expired!\n");
	    }
	}

	print_total_time(opts);

	if (parse_options_get_batch_mode(opts)) {
	    batch_process_some_linkages(label, sent, opts);
	}
	else {
	    process_some_linkages(sent, opts);
	}

	sentence_delete(sent);
	if (external_space_in_use != 0) {
	    fprintf(stderr, "Warning: %d bytes of external space leaked.\n", 
		    external_space_in_use);
	}
    }

    if (parse_options_get_batch_mode(opts)) {
	print_time(opts, "Total");
	fprintf(stderr, 
		"%d error%s.\n", batch_errors, (batch_errors==1) ? "" : "s");
    }

    parsing_space_leaked = space_in_use - dictionary_and_option_space;
    if (parsing_space_leaked != 0) {
        fprintf(stderr, "Warning: %d bytes of space leaked during parsing.\n", 
		parsing_space_leaked);
    }

    parse_options_delete(panic_parse_opts);
    parse_options_delete(opts);
    dictionary_delete(dict);

    if (space_in_use != parsing_space_leaked) {
        fprintf(stderr, 
		"Warning: %d bytes of dictionary and option space leaked.\n", 
		space_in_use - parsing_space_leaked);
    } 
    else if (parsing_space_leaked == 0) {
        fprintf(stderr, "Good news: no space leaked.\n");
    }

    if (external_space_in_use != 0) {
        fprintf(stderr, "Warning: %d bytes of external space leaked.\n", 
		external_space_in_use);
    }

    return 0;
}
