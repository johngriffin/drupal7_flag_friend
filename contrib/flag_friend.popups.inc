<?php
// $Id: flag_friend.popups.inc,v 1.1.2.1 2010/03/15 22:55:27 crea Exp $

/**
 * @file: Popups module integration for the Flag Friend.
 */

// add a little notice to our popups
drupal_add_js(drupal_get_path('module', 'flag_friend') .'/flag_friend.popups.js');

/**
 * Implementation of hook_popups().
 */
function flag_friend_popups() {
  return array(
    'user/*' => array( // any user page
      'span.flag-friend a' => array(
        'afterSubmit' => 'Drupal.flagFriendPopupsAfterSubmit',
      ),
    ),
    'user/*/friends/*' => array( // friend views
      'span.friend a' => array(
        'afterSubmit' => 'Drupal.flagFriendPopupsAfterSubmit',
      ),
    ),
  );
}
