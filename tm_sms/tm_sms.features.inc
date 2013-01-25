<?php

/**
 * @file
 * Contains the functions for the default, included TeleMo SMS features
 *
 * @creation November 2012
 * @author Rod Jackson (RDJ)
 */
 
 
/*
 ******** OPT-IN FEATURES ************
 */

/**
 * Executes the code for the SMS opt-in feature 'promos':
 * Users signed up for this feature will receive information
 * about various promotions, discounts, and other specials
 * that your website and/or store is offering. These promos
 * can be sent out immediately by site admins, or can be
 * setup to be regularly recurring. The text messages sent
 * are intended to not vary among the recipients.
 */
function tm_sms_feature_promos($message) {

	// get the info for the sends
	$signups = _telemo_get_signups_for_feature('sms', 'promos');
	// do each send individually, so we don't stop the process due to one error
	foreach ($signups as $signup) {
		$num = $signup['mobile_num'];
		$timestamp = time();
		$ids_per_api = tmSendSMS($num, '0', $message); // a '0' forces the default 'from' number(s)
		// gotta loop, just in case multiple APIs are enabled
		foreach ($ids_per_api as $ids) {
			// check for errors; if none, record the details
			if (is_array($ids)) {
				db_query("INSERT INTO {telemo_actions} (id, message, timestamp, service_id, feature_id) VALUES ('%s', '%s', %d, 'sms', 'promos')", $ids[0], $message, $timestamp);
				db_query("INSERT INTO {telemo_action_numbers} (action_id, user_id, number) VALUES ('%s', %d, '%s')", $ids[0], $signup['id'], $num);
				db_query("UPDATE {telemo_users} SET last_action_time = %d, last_action_event = 'promos' WHERE id = %d", $timestamp, $signup['id']);
			}
		}
	}
	// record the last public send for use by other features
	variable_set('tm_sms_last_public_sms_message', $message);
	variable_set('tm_sms_last_public_sms_time', $timestamp);

}


/*
 * Executes the code for the SMS opt-in feature 'info':
 * Users signed up for this feature will receive informative
 * alerts that the site admins may broadcast at any time.
 * These messages are generally not intended to have any
 * user-specific elements (such as including the user's
 * name or order number in the message,) nor to be
 * scheduled for broadcast at a future time.
 */
function tm_sms_feature_info($message) {

	// get the info for the sends
	$signups = _telemo_get_signups_for_feature('sms', 'info');
	// do each send individually, so we don't stop the process due to one error
	foreach ($signups as $signup) {
		$num = $signup['mobile_num'];
		$timestamp = time();
		$ids_per_api = tmSendSMS($num, '0', $message); // a '0' forces the default 'from' number(s)
		// gotta loop, just in case multiple APIs are enabled
		foreach ($ids_per_api as $ids) {
			// check for errors; if none, record the details
			if (is_array($ids)) {
				db_query("INSERT INTO {telemo_actions} (id, message, timestamp, service_id, feature_id) VALUES ('%s', '%s', %d, 'sms', 'info')", $ids[0], $message, $timestamp);
				db_query("INSERT INTO {telemo_action_numbers} (action_id, user_id, number) VALUES ('%s', %d, '%s')", $ids[0], $signup['id'], $num);
				db_query("UPDATE {telemo_users} SET last_action_time = %d, last_action_event = 'info' WHERE id = %d", $timestamp, $signup['id']);
			}
		}
	}
	// record the last public send for use by other features
	variable_set('tm_sms_last_public_sms_message', $message);
	variable_set('tm_sms_last_public_sms_time', $timestamp);

}


/**
 * Implements the 'ship' opt-in feature: the user receives an SMS when his order has been 'shipped'.
 */
function tm_sms_feature_ship($message, $data) {

	// extract the info for the send
	$parts = explode(':', $data); // break into 'order=###', 'phone=###' and 'user=###'
	$mobile_num = substr($parts[1], 6); // skip 'phone=' and store '###' into $mobile_num
	$user_id = substr($parts[2], 5); // skip 'user=' and store '###' into $user_id
	// do the send
	$timestamp = time();
	$ids_per_api = tmSendSMS($mobile_num, '0', $message); // a '0' forces the default 'from' number(s)
	// gotta loop, just in case multiple APIs are enabled
	foreach ($ids_per_api as $ids) {
		// check for errors; if none, record the details
		if (is_array($ids)) {
			db_query("INSERT INTO {telemo_actions} (id, message, timestamp, service_id, feature_id, major_data) VALUES ('%s', '%s', %d, 'sms', 'ship', '%s')", $ids[0], $message, $timestamp, $data);
			db_query("INSERT INTO {telemo_action_numbers} (action_id, user_id, number) VALUES ('%s', %d, '%s')", $ids[0], $user_id, $mobile_num);
			db_query("UPDATE {telemo_users} SET last_action_time = %d, last_action_event = 'ship' WHERE id = %d", $timestamp, $user_id);
		}
	}
}


/**
 * Checks to see if the 'ship' opt-in feature needs to send an SMS; also executes all pending SMS sends
 */
function tm_sms_feature_ship_check($form, &$form_state) {

	// make sure this feature is enabled & we're not in debug mode before doing anything
	$continue = TRUE;
	if (!variable_get('tm_sms_feature_ship_enabled', FALSE)) {
		$continue = FALSE;
	}
	//elseif (variable_get('telemo_debug_mode', FALSE) && !user_access('access administration menu')) {
	//	$continue = FALSE;
	//}
	
	// if continuing, we start gathering data
	if ($continue) {
		// extract the data needed to perform the checks
		$order_id = $form_state['values']['order_id'];
		$status_id = $form_state['values']['status'];
		
		// fetch the rest of the data needed from the database
		$record = db_fetch_array(db_query("SELECT uid, order_total, delivery_first_name FROM {uc_orders} WHERE order_id = %d", $order_id));
		$order_total = uc_price($record['order_total'], array('revision' => 'formatted'));
		$user_id = $record['uid'];
		$delivery_name = $record['delivery_first_name'];
		if ($delivery_name == '') {
			$user_name = 'CUSTOMER';
		}
		else {
			$user_name = strtolower($delivery_name);
			$user_name[0] = strtoupper($user_name[0]);
		}
		
		// check to see if there's a TeleMo user whose ID is the negative of this order first
		$record = db_fetch_array(db_query("SELECT mobile_num, signups FROM {telemo_users} WHERE id = %d", -$order_id));
		if ($record !== FALSE) {
			// we have such a record; check to see if the user has a separate record
			$user_record = db_fetch_array(db_query("SELECT mobile_num, signups FROM {telemo_users} WHERE id = %d", $user_id));
			// if not, convert the order's record into one specific to the user
			if ($user_record === FALSE) {
				db_query("UPDATE {telemo_users} SET id = %d WHERE id = %d", $user_id, -$order_id);
				$id = $user_id;
			}
			// else, if there IS a second record, use the order's record data, and remember that we are doing so
			else {
				$id = -$order_id;
			}
		}
		// if there's no record for the order specifically, check to see if there's a record for the user
		else {
			$user_record = db_fetch_array(db_query("SELECT mobile_num, signups FROM {telemo_users} WHERE id = %d", $user_id));
			// if there is, use that record as the operative record
			if ($user_record !== FALSE) {
				$record = $user_record;
				$id = $user_id;
			}
			// otherwise, neither the user nor the order is signed up; we won't do the send
			else {
				$continue = FALSE;
				$id = 0;
			}
		}
	} // end of check to see whether we should start gathering data

	// if we're continuing, we have enough data to use for verification
	if ($continue) {
		// we have a record; let's make sure the user is signed up for this feature
		$user_signups = $record['signups'];
		$mobile_num = $record['mobile_num'];
		$tagged_feature_id = ':sms~ship;'; // this is how features are stored in the field
		$pos = strpos($user_signups, $tagged_feature_id);
		if ($pos === FALSE) {
			$continue = FALSE;
		}
		// make sure the status is set or being set to 'shipped';
		// look for 'shipped' in the status title, or just a status
		// 'completed' (in the 'completed' state)
		$status_title = uc_order_status_data($status_id, 'title');
		$pos = strpos(strtolower($status_title), 'shipped');
		if (($status_title != 'completed') && ($pos === FALSE)) {
			$continue = FALSE;
		}
	}
	
	// if we're still continuing, then the feature is enabled, the user is
	// signed up for it, and the status is set to 'Shipped'. But....
	if ($continue) {
		// Has an SMS for this order's shipping been sent
		// aready? Check records up to a week old.
		$t_minus_one_week = time() - 604800;
		$db_data = db_query("SELECT major_data FROM {telemo_actions} WHERE feature_id = 'ship' AND timestamp > %d", $t_minus_one_week);
		$found = FALSE;
		while ($data = db_result($db_data)) {
			$parts = explode(':', $data); // break into 'order=###', 'phone=###' and 'user=###'
			$num = substr($parts[0], 6); // skip 'order=' and store '###' into $num
			if ($num == $order_id) {
				$found = TRUE;
				break;
			}
		}
		// if we found a prior send for the order, don't continue
		if ($found) {
			$continue = FALSE;
		}
		else {
			// check if a send for this order is pending (may not be immediate sends)
			$db_data = db_query("SELECT major_data FROM {telemo_future_actions} WHERE feature_id = 'ship'");
			while ($data = db_result($db_data)) {
				$parts = explode(':', $data); // break into 'order=###', 'phone=###' and 'user=###'
				$num = substr($parts[0], 6); // skip 'order=' and store '###' into $num
				if ($num == $order_id) {
					$found = TRUE;
					break;
				}
			}
			if ($found) {
				$continue = FALSE;
			}
		}
	}
	
	// still continuing? Then we passed all checks; do the send
	if ($continue) {
		// setup a future SMS send, for right now, non-recurring
		$t = time();
		// get info for carrier and tracking number from the order
		$shipment = db_fetch_array(db_query("SELECT carrier, tracking_number FROM {uc_shipments} WHERE order_id = %d", $order_id));
		$carrier = $shipment['carrier'];
		$tracking_num = $shipment['tracking_number'];
		// find and replace the variables in the message
		$message = variable_get('tm_sms_feature_ship_message', TM_SMS_DEFAULT_SHIP_MESSAGE);
		$message = str_replace('!name', $user_name, $message);
		$message = str_replace('!order', $order_id, $message);
		$message = str_replace('!total', $order_total, $message);
		$message = str_replace('!org', variable_get('tm_sms_org_name', ''), $message);
		$message = str_replace('!carrier', $carrier, $message);
		$message = str_replace('!track', $tracking_num, $message);
		// for the 'ship' feature we need to bundle the order ID,
		// the mobile number and the user ID to send to
		$data = 'order=' . $order_id . ':phone=' . $mobile_num . ':user=' . $user_id;
		// store the future send
		db_query("INSERT INTO {telemo_future_actions} (action_time, recurrence, service_id, feature_id, message, major_data) VALUES (%d, 'none', 'sms', 'ship', '%s', '%s')", $t, $message, $data);
		// if there were two records (one for the user, one for the order), get rid of the order record now
		if ($id == -$order_id) {
			db_query("DELETE FROM {telemo_users} WHERE id = %d", -$order_id);
		}
	}

	// as cleanup for the 'ship' feature, remove any negative-ID'd users created more than 2 weeks ago
	$t_minus_two_weeks = time() - 1209600;
	db_query("DELETE FROM {telemo_users} WHERE id < 0 AND creation < %d", $t_minus_two_weeks);
	
	// execute all pending sends (including this one, if created)
	telemo_execute_next_scheduled_actions();
	
} // end function - tm_sms_feature_ship_check


/*
 ***************** NON-OPT-IN FEATURES ***************
 */

/**
 * Executes the code for the SMS response feature 'stop'.
 * This feature has three variations: STOP, STOP feature,
 * and STOP ALL. If STOP is received, the user associated
 * with the number the SMS was received from will be
 * removed from the signup list of the feature that last
 * sent that user a text message. If STOP ALL is received,
 * the user will be removed from all feature signups. If
 * STOP is received, followed by the 'id' of one or more
 * specific features, the user will be removed from those
 * specific feature's signup lists.
 */
/*function tm_sms_response_feature_stop($from, $to, $words) {

	// first, identify the user who sent the SMS
	$data = db_fetch_array(db_query("SELECT id, signups, last_action_time, last_action_event FROM {telemo_users} WHERE mobile_num = '%s'", $from));
	if ($data === FALSE) {
		if (strlen($from) > 10) {
			$altered_from = substr($from, 1);
		}
		else {
			$altered_from = '1' . $from;
		}
		$data = db_fetch_array(db_query("SELECT id, signups, last_action_time, last_action_event FROM {telemo_users} WHERE mobile_num = '%s'", $altered_from));
	}
	
	// if we couldn't identify the user, just exit the function now
	if ($data === FALSE) {
		return FALSE;
	}
	
	// prepare to update the user's record
	$user_id = $data['id'];
	$signups = $data['signups'];
	$last_sms = array('time' => $data['last_action_time'], 'feature' => $data['last_action_event']);
	$unsignups = 0;
	
	// see if there were any further words; if so, unsignup from the proper service(s) and exit
	if (count($words) > 0) {
		// loop through all the words, unsignup from each
		foreach ($words as $word) {
			// if the word is ALL, just unsignup from all features and move along
			if ($word == 'ALL') {
				$signups = '';
				$unsignups = 'all';
				break;
			}
			else {
				// convert the word into a tagged feature id and replace it with an empty string
				$tagged_id = ':sms~' . strtolower($word) . ';';
				$updated_signups = str_replace($tagged_id, '', $signups);
				if ($updated_signups != $signups) {
					$unsignups++;
					$signups = $updated_signups;
				}
			}
		}
	}
	else {
		// no further words were included with the STOP text;
		// we found the send; unsignup for the feature
		$tagged_id = ':sms~' . $last_sms['feature'] . ';';
		$updated_signups = str_replace($tagged_id, '', $signups);
		if ($updated_signups != $signups) {
			$unsignups++;
			$signups = $updated_signups;
		}
	} // end of check for further words
	
	// update the user's record
	db_query("UPDATE {telemo_users} SET signups = '%s' WHERE id = %d", $signups, $user_id);
	
	// let them know they were successful (if they were), unless it was STOP ALL
	if (($unsignups != 0) && ($unsignups != 'all')) {
		$ftext = ($unsignups === 1) ? 'feature' : 'features';
		$unsignups = ($unsignups === 1) ? 'a' : $unsignups;
		$message = 'STOP was successful; you have been unsubscribed from ' . $unsignups . ' text message ' . $ftext . '.';
		// do the send
		$timestamp = time();
		$ids_per_api = tmSendSMS($from, '0', $message);
		foreach ($ids_per_api as $ids) {
			// create and store a record of the send
			db_query("INSERT INTO {telemo_actions} (id, message, timestamp, feature_id) VALUES (%d, '%s', %d, 'stop', '%s')", $ids[0], $message, $timestamp);
			// record the user and the number we sent to
			db_query("INSERT INTO {telemo_action_numbers} (action_id, user_id, number) VALUES (%d, %d, '%s')", $ids[0], $user_id, $from);
		}
	}
	
	// we are done
	return $unsignups;
	
}
*/



/*
 ******************* MISC. RELATED ROUTINES ***************
 */

/**
 * Implements hook_checkout_pane for Ubercart; allows user opt-in of SMS features during checkout.
 */
function tm_sms_checkout_pane() {

  $panes[] = array(
    'id' => 'tm_sms',
    'title' => t('SMS notifications opt-in'),
    'desc' => t('Allow the user to opt-in to various SMS features.'),
    'callback' => 'uc_checkout_pane_tm_sms',
    'weight' => 7,
  );

  return $panes;

}


/**
 * Interacts with Ubercart's checkout system to allow a customer to opt-in
 * to various CallFire SMS features during checkout.
 */
function uc_checkout_pane_tm_sms($op, &$arg1, $arg2) {

	global $user;
	
	switch ($op) {
	
		case 'view':

			$contents = array();

			// get the user ID
			$user_id = $user->uid;
			
			// if no user ID, we need to keep track of what they sign up for;
			// store a negative timestamp for now, replace with order # later
			if ($user_id <= 0) {
				if (isset($_SESSION['cart_order'])) {
					$user_id = -intval($_SESSION['cart_order']);
				}
				else {
					$user_id = -time();
				}
			}
			
			// if TeleMo is in debug mode, but this isn't an admin, skip out
			if (variable_get('telemo_debug_mode', 0) && !user_access('access administration menu')) {
				return $contents;
			}

			// load the user's signed-up-for features & phone num; if none, give blank values
			$data = db_fetch_array(db_query("SELECT mobile_num, signups FROM {telemo_users} WHERE id = %d", $user_id));
			if ($data === FALSE) {
				$mobile_num = '';
				$user_signups = '';
			}
			else {
				$record = $data;
				$user_signups = $record['signups'];
				$mobile_num = $record['mobile_num'];
			}

			// start adding the form elements

 			$contents['telemo_user_id_passthru'] = array(
 				'#type' => 'hidden',
 				'#value' => $user_id
 			);

    	$contents['telemo_mobile_num'] = array(
				'#type' => 'textfield',
				'#title' => t('Mobile number for texting'),
				'#default_value' => _telemo_display_mobile_num($mobile_num),
			);
    
			// loop through all SMS features
	    $all_features = tm_sms_all_features();
   		foreach ($all_features as $feature) {

				// if the feature is enabled & an opt-in feature, add it to the form
				$feature_id = $feature['id'];
				if ($feature['enabled'] && $feature['optin']) {
	  		  $tagged_feature_id = ':sms~' . $feature_id . ';';
	  		  $feature_id_signedup = 'tm_sms_feature_' . $feature_id . '_signedup';
					$pos = strpos($user_signups, $tagged_feature_id);
					$contents[$feature_id_signedup] = array(
						'#type' => 'checkbox',
						'#title' => '<strong>' . $feature['name'] . ':</strong> ' . $feature['text'],
						'#default_value' => !($pos === FALSE),
					);
     		}
      		
   		}
   		
   		$description = variable_get('tm_sms_signup_message', TM_SMS_SIGNUP_PROMPT_SETTING);
   		
			$pane = array('description' => $description, 'contents' => $contents);
			
			return $pane;

	} // end switch based on $op
	
}
