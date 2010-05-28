<?php
// $Id: flag_friend_handler_filters.inc,v 1.1.2.1 2010/03/15 22:55:27 crea Exp $

/**
 * Filter handler allowing to filter by flags (Flag Friend relationship types).
 */

class flag_friend_handler_filter_flag extends views_handler_filter_many_to_one {
  function construct() {
    parent::construct();
    $this->value_title = t('Options');
    
    $flags = flag_get_flags('user');
    $options = array();
    foreach ($flags as $flag) {
      $options[$flag->fid] = $flag->get_title();
    }
    $this->value_options = $options;
  }
}
