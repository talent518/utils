#!/usr/bin/env php
<?php

$tokens = token_get_all(file_get_contents($_SERVER['argv'][1]??__FILE__));

//var_dump(array_map(function($a) {if(is_array($a)) $a[0] = token_name($a[0]); return $a;},$tokens));

$isFunc = false;
$isVar = false;
$closed = "\033[0m";

foreach($tokens as $i=>$token) {
	if(is_array($token)) {
		@list($token, $cont, $line) = $token;
		
		if($isVar) {
			echo $cont;
			continue;
		}

		switch($token) {
			case T_STRING:
				if(preg_match('/^(true|false)$/i', $cont)) {
					$token = T_LNUMBER;
				} elseif(preg_match('/^(int|bool|boolean|float|real|double|string)$/i', $cont)) {
					$token = T_INT_CAST;
				}
				break;
			case T_ARRAY:
				if($isFunc) {
					$token = T_ARRAY_CAST;
				}
				break;
			case T_FUNCTION:
				$isFunc = true;
				break;
			default:
				break;
		}

		switch($token) {
			case T_BAD_CHARACTER:
				echo "\033[41;30m$cont\033[0m$closed";
				break;
			case T_EXIT:
				if(!strcasecmp($cont, 'die')) {
					echo $cont;
					break;
				}
			case T_IF:
			case T_ELSEIF:
			case T_ELSE:
			case T_SWITCH:
			case T_CASE:
			case T_DEFAULT:
			case T_BREAK:
			case T_FOREACH:
			case T_AS:
			case T_FOR:
			case T_DO:
			case T_WHILE:
			case T_CONTINUE:
			case T_DECLARE:
			case T_ENDDECLARE:
			case T_ENDFOR:
			case T_ENDFOREACH:
			case T_ENDIF:
			case T_ENDSWITCH:
			case T_ENDWHILE:
			case T_GLOBAL:
			case T_LIST:
			case T_EMPTY:
			case T_ISSET:
			case T_UNSET:
			case T_ECHO:
			case T_INCLUDE:
			case T_INCLUDE_ONCE:
			case T_REQUIRE:
			case T_REQUIRE_ONCE:
			case T_ARRAY:
			case T_EVAL:
				echo "\033[32m$cont$closed";
				break;
			case T_START_HEREDOC:
				$closed = "\033[31m";
				echo "\033[32m$cont$closed";
				break;
			case T_END_HEREDOC:
				$closed = "\033[0m";
				echo "\033[32m$cont$closed";
				break;

			case T_BOOL_CAST:
			case T_ARRAY_CAST:
			case T_DOUBLE_CAST:
			case T_INT_CAST:
			case T_OBJECT_CAST:
			case T_STRING_CAST:
			case T_UNSET_CAST:
				echo "\033[36m$cont$closed";
				break;

			case T_NAMESPACE:
			case T_USE:
			case T_CLASS:
			case T_EXTENDS:
			case T_INTERFACE:
			case T_IMPLEMENTS:
			case T_TRAIT:
			case T_ABSTRACT:
			case T_FUNCTION:
			
			case T_STATIC:
			case T_PUBLIC:
			case T_PROTECTED:
			case T_PRIVATE:
			case T_FINAL:
			case T_CONST:
			case T_VAR:
			
			case T_RETURN:
			case T_TRY:
			case T_CATCH:
			case T_THROW:
			case T_FINALLY:
			
			case T_NEW:
			case T_LOGICAL_AND:
			case T_LOGICAL_OR:
			case T_LOGICAL_XOR:
				echo "\033[34m$cont$closed";
				break;

			case T_CURLY_OPEN:
			case T_DOLLAR_OPEN_CURLY_BRACES:
				$isVar = true;
				echo "\033[35m$cont";
				break;
			case T_VARIABLE:
			case T_STRING_VARNAME:
				echo "\033[35m$cont$closed";
				break;

			case T_ENCAPSED_AND_WHITESPACE:
			case T_CONSTANT_ENCAPSED_STRING:
				echo "\033[31m$cont$closed";
				break;

			case T_DOUBLE_COLON:
			case T_DOUBLE_ARROW:
			case T_OBJECT_OPERATOR:
			case T_CURLY_OPEN:
				//echo "\033[33m$cont$closed";
				echo $cont;
				break;

			case T_FILE:
			case T_LINE:
			case T_FUNC_C:
			case T_CLASS_C:
			case T_LNUMBER:
			case T_DNUMBER:
				echo "\033[33m$cont$closed";
				break;

			case T_COMMENT:
			case T_DOC_COMMENT:
				echo "\033[30m$cont$closed";
				break;

			//case T_STRING:
			//	echo "\033[40;37m$cont$closed";
			//	break;

			default:
				switch($token) {
					case T_STRING:
					case T_WHITESPACE:
					case T_ENCAPSED_AND_WHITESPACE:
						break;
					default:
						//echo token_name($token);
						break;
				}
				echo $cont;
				break;
		}
	} else {
		switch($token) {
			case '"':
				echo "\033[31m\"$closed";
				break;
			case '{':
				$isFunc = false;
				echo '{';
				break;
			default:
				echo $token;
				if($isVar && $token === '}') {
					echo $closed;
					$isVar = false;
				}
				break;
		}
	}
}

