/* preprocessor directives: #warning stays a warning under warning_errors
   (3.0.1), #ident is skipped, an unknown directive is an error */
#pragma warning_errors on
#warning this is only a warning
#ident "version 1"
#pragma warning_errors off
#warning plain warning
#bogus directive
int x;
