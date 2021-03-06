<?php

/**
 * Form for confirming the flag_friend operations.
 * 
 * We perform most checks in menu access callback so can have less logics here.
 */
function flag_friend_confirm(&$form_state, $link_type, $op, $flag_name, $content_id) {
  global $user;
  $flag = flag_get_flag($flag_name);
  $status = flag_friend_relationship_status($flag, $user->uid, $content_id);
  $action = _flag_friend_get_action($op, $status);
  $form = array();
  $form['#submit'] = array('flag_friend_confirm_submit');
  
  // Store data in the form.
  $form['link_type'] = array(
    '#type' => 'value',
    '#value' => $link_type,
  );
  // Not "op" cause that's used by the form itself.
  $form['ff_op'] = array(
    '#type' => 'value',
    '#value' => $op,
  );
  $form['action'] = array(
    '#type' => 'value',
    '#value' => $action,
  );
  $form['flag_name'] = array(
    '#type' => 'value',
    '#value' => $flag_name,
  );
  $form['flag'] = array(
    '#type' => 'value',
    '#value' => &$flag,
  );  
  $form['content_id'] = array(
    '#type' => 'value',
    '#value' => $content_id,
  );
    
  if ($op == 'request') {
    $form['relationship_request_message'] = array(
      '#type' => 'textarea',
      '#title' => $flag->get_label('request_msg_title', $content_id),
      '#description' => $flag->get_label('request_msg_desc', $content_id),
      '#cols' => 60,
      '#rows' => 5,
      '#default_value' => $flag->get_label('request_msg', $content_id),
    );
  }
  
  $path = isset($_GET['destination']) ? $_GET['destination'] : '<front>';
  $question    = $flag->get_label("${op}_question", $content_id);
  $yes         = $flag->get_label("${op}_yes", $content_id);
  $no          = $flag->get_label("${op}_no", $content_id);
  $description = '';
  $name        = 'confirm';
  $form = confirm_form($form, $question, $path, $description, $yes, $no, $name);
  $form['#theme'] = "flag_friend_confirm_form";
  return $form;
}

/**
 * Submit handler. Similar to flag_confirm_submit() but saves some additional
 * info such as message.
 * 
 * Using transactions because the whole operation is not atomic and 
 * DB could end up in an inconsistent state.
 */
function flag_friend_confirm_submit($form, &$form_state) {
  global $user, $flag_friend_request_message, $flag_friend_rollback;
  $op = $form_state['values']['ff_op'];
  $action = $form_state['values']['action'];
  $flag  = $form_state['values']['flag'];
  $content_id = $form_state['values']['content_id'];
  $token_context_id = $content_id;
  
  // Swap users incase this is deny
  if ($op == 'deny') {
    $account = user_load($content_id);
    $content_id = $GLOBALS['user']->uid;
  }
  else {
    $account =& $GLOBALS['user'];
  }
  
  // Store message in a global variable so we can access it in hook_flag().
  if (isset($form_state['values']['relationship_request_message'])) {
    $flag_friend_request_message = $form_state['values']['relationship_request_message'];
  }

  // Flag/unflag.
  _flag_friend_transaction('BEGIN');
  if (!$flag->flag($action, $content_id, $account) || isset($flag_friend_rollback)) {
    _flag_friend_transaction('ROLLBACK');
    drupal_set_message($flag->get_label("${op}_failed_message", $token_context_id));
  }
  else {
    _flag_friend_transaction('COMMIT');
    drupal_set_message($flag->get_label("${op}_success_message", $token_context_id));      
  }
}


/**
 * Private version of flag_friend_form_flag_form_alter() for convenience.
 */
function _flag_friend_form_flag_form_alter(&$form, &$form_state) {
  $flag = $form['#flag'];
  // Remove our link type manually in case it's not applicable and terminate
  // early.
  // TODO: This may become unneeded. See http://drupal.org/node/721474
  if ($flag->content_type != 'user') {
    unset($form['display']['link_type']['#options']['msg_confirm']);
    return;
  }
  
  // We need to apply link options as default values manually in case this is
  // new Flag. Need to be careful to not overwrite saved values.
  foreach (_flag_friend_default_link_options() as $key => $value) {
    if (!isset($flag->$key)) {
      $flag->$key = $value;
    }
  }
  // Insert warning.
  $warning  = t('<strong>Note that message settings have no effect with Flag Friend link types. Flag Friend has own messages configured in <em>Flag Friend options</em> below.</strong>');
  $warning .= '<br/><br/>';
  $form['messages']['flag_friend_description'] = array(
    '#value' => $warning,
    '#weight' => -10,
  );
  // We could remove "required" validation but that would break Flag UX for 
  // other user flags. Thus this is most simple we can do.
  $note = t('For Flag Friend flags insert random garbage here.');
  $form['messages']['flag_short']['#description'] .= ' ' . $note;
  $form['messages']['unflag_short']['#description'] .= ' ' . $note;

  $fieldset = array(
    '#type' => 'fieldset',
  );
  
  $form['flag_friend_options'] = $fieldset + array(
    '#title' => t('Flag Friend options'),
    '#weight' => 20,
  );
  $form['flag_friend_options']['flag_friend_notes'] = array(
    '#value' => t("Note: these are Flag Friend module options. You don't need any of these for simple user flags."),
  );
  
  $fieldset['#collapsible'] = TRUE;
  $fieldset['#collapsed'] = TRUE;
  
  // Request
  $form['flag_friend_options']['request_fieldset'] = $fieldset + array(
    '#title' => t('Relationship request'),
  );
  $form['flag_friend_options']['request_fieldset']['request_short'] = array(
    '#type' => 'textfield',
    '#title' => t('Request link text'),
    '#default_value' => $flag->request_short,
    '#description' => t('Text displayed as the link.'),
    '#access' => empty($flag->locked['request_short']),
  );
  $form['flag_friend_options']['request_fieldset']['request_long'] = array(
    '#type' => 'textfield',
    '#title' => t('Request link description'),
    '#default_value' => $flag->request_long,
    '#description' => t('Text displayed as the <em>title</em> attribute of the link.'),
    '#access' => empty($flag->locked['request_long']),
  );
  $form['flag_friend_options']['request_fieldset']['request_question'] = array(
    '#type' => 'textfield',
    '#title' => t('Request question'),
    '#default_value' => $flag->request_question,
    '#description' => t('Text displayed as the confirmation question in the confirmation form.'),
    '#access' => empty($flag->locked['request_question']),
  );
  $form['flag_friend_options']['request_fieldset']['request_yes'] = array(
    '#type' => 'textfield',
    '#title' => t('Request <em>yes</em> answer text'),
    '#default_value' => $flag->request_yes,
    '#description' => t('Text displayed as <em>Yes</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['request_yes']),
  );
  $form['flag_friend_options']['request_fieldset']['request_no'] = array(
    '#type' => 'textfield',
    '#title' => t('Request <em>no</em> answer text'),
    '#default_value' => $flag->request_no,
    '#description' => t('Text displayed as <em>No</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['request_no']),
  );
  $form['flag_friend_options']['request_fieldset']['request_msg'] = array(
    '#type' => 'textfield',
    '#title' => t('Request default message text'),
    '#default_value' => $flag->request_msg,
    '#description' => t('Default text displayed inside message textarea in the confirmation form if the user has requested relationship.'),
    '#access' => empty($flag->locked['request_msg']),
  );
  $form['flag_friend_options']['request_fieldset']['request_msg_title'] = array(
    '#type' => 'textfield',
    '#title' => t('Request message title'),
    '#default_value' => $flag->request_msg_title,
    '#description' => t('Text displayed as title of message textarea in the confirmation form if the user has requested relationship.'),
    '#access' => empty($flag->locked['request_msg_title']),
  );
  $form['flag_friend_options']['request_fieldset']['request_msg_desc'] = array(
    '#type' => 'textfield',
    '#title' => t('Request message description'),
    '#default_value' => $flag->request_msg_desc,
    '#description' => t('Text displayed as description of message textarea in the confirmation form if the user has requested relationship.'),
    '#access' => empty($flag->locked['request_msg_desc']),
  );
  $form['flag_friend_options']['request_fieldset']['request_success_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Request success message'),
    '#default_value' => $flag->request_success_message,
    '#description' => t('Text displayed on success of relationship request operation.'),
    '#access' => empty($flag->locked['request_success_message']),
  );
  $form['flag_friend_options']['request_fieldset']['request_failed_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Request failure message'),
    '#default_value' => $flag->request_failed_message,
    '#description' => t('Text displayed on failure of relationship request operation.'),
    '#access' => empty($flag->locked['request_failed_message']),
  ); 
  
  // Cancel pending
  $form['flag_friend_options']['cancel_fieldset'] = $fieldset + array(
    '#title' => t('Relationship request cancellation'),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_short'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel link text'),
    '#default_value' => $flag->cancel_short,
    '#description' => t('Text displayed as the link.'),
    '#access' => empty($flag->locked['cancel_short']),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_long'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel link description'),
    '#default_value' => $flag->cancel_long,
    '#description' => t('Text displayed as the <em>title</em> attribute of the link.'),
    '#access' => empty($flag->locked['cancel_long']),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_question'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel question'),
    '#default_value' => $flag->cancel_question,
    '#description' => t('Text displayed as the confirmation question in the confirmation form.'),
    '#access' => empty($flag->locked['cancel_question']),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_yes'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel <em>yes</em> answer text'),
    '#default_value' => $flag->cancel_yes,
    '#description' => t('Text displayed as <em>Yes</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['cancel_yes']),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_no'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel <em>no</em> answer text'),
    '#default_value' => $flag->cancel_no,
    '#description' => t('Text displayed as <em>No</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['cancel_no']),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_success_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel success message'),
    '#default_value' => $flag->cancel_success_message,
    '#description' => t('Text displayed on success of cancellation of relationship request.'),
    '#access' => empty($flag->locked['cancel_success_message']),
  );
  $form['flag_friend_options']['cancel_fieldset']['cancel_failed_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Cancel failure message'),
    '#default_value' => $flag->cancel_failed_message,
    '#description' => t('Text displayed on failure of cancellation of relationship request.'),
    '#access' => empty($flag->locked['cancel_failed_message']),
  );
  
  // Approve request
  $form['flag_friend_options']['approve_fieldset'] = $fieldset + array(
    '#title' => t('Relationship request approval'),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_short'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve link text'),
    '#default_value' => $flag->approve_short,
    '#description' => t('Text displayed as the link.'),
    '#access' => empty($flag->locked['approve_short']),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_long'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve link description'),
    '#default_value' => $flag->approve_long,
    '#description' => t('Text displayed as the <em>title</em> attribute of the link.'),
    '#access' => empty($flag->locked['approve_long']),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_question'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve question'),
    '#default_value' => $flag->approve_question,
    '#description' => t('Text displayed as the confirmation question in the confirmation form.'),
    '#access' => empty($flag->locked['approve_question']),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_yes'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve <em>yes</em> answer text'),
    '#default_value' => $flag->approve_yes,
    '#description' => t('Text displayed as <em>Yes</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['approve_yes']),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_no'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve <em>no</em> answer text'),
    '#default_value' => $flag->approve_no,
    '#description' => t('Text displayed as <em>No</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['approve_no']),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_success_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve success message'),
    '#default_value' => $flag->approve_success_message,
    '#description' => t('Text displayed on success of approvement of relationship request.'),
    '#access' => empty($flag->locked['approve_success_message']),
  );
  $form['flag_friend_options']['approve_fieldset']['approve_failed_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Approve failure message'),
    '#default_value' => $flag->approve_failed_message,
    '#description' => t('Text displayed on failure of approvement of relationship request.'),
    '#access' => empty($flag->locked['approve_failed_message']),
  );
  
  // Deny request
  $form['flag_friend_options']['deny_fieldset'] = $fieldset + array(
    '#title' => t('Relationship request denial'),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_short'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny link text'),
    '#default_value' => $flag->deny_short,
    '#description' => t('Text displayed as the link.'),
    '#access' => empty($flag->locked['deny_short']),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_long'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny link description'),
    '#default_value' => $flag->deny_long,
    '#description' => t('Text displayed as the <em>title</em> attribute of the link.'),
    '#access' => empty($flag->locked['deny_long']),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_question'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny question'),
    '#default_value' => $flag->deny_question,
    '#description' => t('Text displayed as the confirmation question in the confirmation form.'),
    '#access' => empty($flag->locked['deny_question']),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_yes'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny <em>yes</em> answer text'),
    '#default_value' => $flag->deny_yes,
    '#description' => t('Text displayed as <em>Yes</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['deny_yes']),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_no'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny <em>no</em> answer text'),
    '#default_value' => $flag->deny_no,
    '#description' => t('Text displayed as <em>No</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['deny_no']),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_success_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny success message'),
    '#default_value' => $flag->deny_success_message,
    '#description' => t('Text displayed on success of denial of relationship request.'),
    '#access' => empty($flag->locked['deny_success_message']),
  );
  $form['flag_friend_options']['deny_fieldset']['deny_failed_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Deny failure message'),
    '#default_value' => $flag->deny_failed_message,
    '#description' => t('Text displayed on failure of denial of relationship request.'),
    '#access' => empty($flag->locked['deny_failed_message']),
  );
  
  // Remove
  $form['flag_friend_options']['remove_fieldset'] = $fieldset + array(
    '#title' => t('Relationship removal'),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_short'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove link text'),
    '#default_value' => $flag->remove_short,
    '#description' => t('Text displayed as the link.'),
    '#access' => empty($flag->locked['remove_short']),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_long'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove link description'),
    '#default_value' => $flag->remove_long,
    '#description' => t('Text displayed as the <em>title</em> attribute of the link.'),
    '#access' => empty($flag->locked['remove_long']),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_question'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove question'),
    '#default_value' => $flag->remove_question,
    '#description' => t('Text displayed as the confirmation question in the confirmation form.'),
    '#access' => empty($flag->locked['remove_question']),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_yes'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove <em>yes</em> answer text'),
    '#default_value' => $flag->remove_yes,
    '#description' => t('Text displayed as <em>Yes</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['remove_yes']),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_no'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove <em>no</em> answer text'),
    '#default_value' => $flag->remove_no,
    '#description' => t('Text displayed as <em>No</em> answer in the confirmation form.'),
    '#access' => empty($flag->locked['remove_no']),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_success_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove success message'),
    '#default_value' => $flag->remove_success_message,
    '#description' => t('Text displayed on success of relationship removal.'),
    '#access' => empty($flag->locked['remove_success_message']),
  );
  $form['flag_friend_options']['remove_fieldset']['remove_failed_message'] = array(
    '#type' => 'textfield',
    '#title' => t('Remove failure message'),
    '#default_value' => $flag->remove_failed_message,
    '#description' => t('Text displayed on failure of relationship removal.'),
    '#access' => empty($flag->locked['remove_failed_message']),
  );
  
  uasort($form, 'element_sort');
}

/**
 * Theme function for Flag Friend relationship confirmation forms.
 */
function theme_flag_friend_confirm_form($form) {
  return drupal_render($form);
}
