<?php
// $Id: flag_friend_plugin_argument_validate_user_current.inc,v 1.1.2.1 2010/03/15 22:55:27 crea Exp $

/**
 * @file
 * Contains the "User: current user" argument validator plugin.
 */


/**
 * Checks that user provided in argument is the current one.  
 *
 * @ingroup views_argument_validate_plugins
 */
class flag_friend_plugin_argument_validate_user_current extends views_plugin_argument_validate {
  function validate_argument($arg) {
    return ($arg === $GLOBALS['user']->uid); 
  }
}
