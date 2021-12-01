import argparse
import logging
import os
#import sys
import time
import datetime
import traceback
import pprint
import urllib.request

import win32serviceutil     # these are to manage Windows services
import win32service

import botocore             # AWS Api
import boto3


# debug constants
DEBUG_SKIP_BALANCER_HANDLING = False
DEBUG_ALLOW_EXT_NODE_FOR_BALANCER_TESTING = False
DEBUG_SKIP_DOWNLOAD_FROM_S3 = False
DEBUG_REMOVE_QUOTES_FROM_ARGS = True

# Configuration constants
WAIT_TARGET_DEREGISTERED_CHECK_SECS = 30.0
WAIT_TARGET_DEREGISTERED_CHECK_MAX_NUM_TRIES = 12
WAIT_TARGET_REGISTERED_CHECK_SECS = WAIT_TARGET_DEREGISTERED_CHECK_SECS
WAIT_TARGET_REGISTERED_CHECK_MAX_NUM_TRIES = WAIT_TARGET_DEREGISTERED_CHECK_MAX_NUM_TRIES

WAIT_SERVICE_STOPPED_CHECK_SECS = 3.0
WAIT_SERVICE_STOPPED_CHECK_MAX_NUM_TRIES = 10
WAIT_SERVICE_RUNNING_CHECK_SECS = WAIT_SERVICE_STOPPED_CHECK_SECS
WAIT_SERVICE_RUNNING_CHECK_MAX_NUM_TRIES = WAIT_SERVICE_STOPPED_CHECK_MAX_NUM_TRIES

# instance a pretty printer, for the development at least
pp = pprint.PrettyPrinter(indent=4)

# sync'ed with generateDiscrete
ERROR_EXIT = -1
OK_EXIT = 0
WARN_EXIT = 1

# boto3 related global variables
session = None          # boto3 session
elbv2 = None            # client for using elbv2 service


def update_this_server_gdb(loadbalancer_name, service_name, gdbfolder_old, gdbfolder_new, gdb_dest_dir, gdb_s3uri_new):
    """Updates the GDB file in THIS server

    :param str loadbalancer_name: name of the load balancer in front of ESRI servers
    :param str service_name:      name of the ESRI service to be stopped and later restarted
    :param str gdbfolder_old:     name of the GDB file currently used, the one to be replaced
    :param str gdbfolder_new:     name of the new GDB file to be put into production
    :param str gdb_dest_dir:      name of the directory where GDB folders are located
    :param str gdb_s3uri_new:     S3 URI where download the new GDB file from
    """

    retVal, loadbalancer_arn = get_loadbalancer_arn( loadbalancer_name )
    if retVal == False: return False

    retVal, target_group_arn, target_type_is_IP = get_target_group_info( loadbalancer_arn )
    if retVal == False: return False

    retVal, target_hosts = get_target_hosts( target_group_arn )
    if retVal == True:
        logging.info(f"Retrieved list of target hosts: {' '.join(target_hosts)}")
    else: return False

    # get the ID for THIS server we're running at, it can be an AWS instance-id or an IP address
    node_id = None
    instance_id = None
    try:
        instance_id = urllib.request.urlopen('http://169.254.169.254/latest/meta-data/instance-id').read().decode()
    except:
        logging.error('Could not retrieve this node-id. Aborting.')

        # development hack !!
        if DEBUG_ALLOW_EXT_NODE_FOR_BALANCER_TESTING == True:
            instance_id = target_hosts[0] if target_type_is_IP == False else 'i-07432400bd9f07a40';
            logging.error(f'WATCH!!! using an external node {node_id} to test load balancing!')

        if instance_id == None:
            return False

    # We've got the instance-id. If TargetGroup's TargetType is IP, then we need an IP address.
    if target_type_is_IP == True:
        retVal, node_id = get_best_candidate_ip_address(instance_id, target_hosts)
        if retVal == False:
            logging.error(f'Could not get a good IP candidate for instance {instance_id}')
            return False
        else:
            logging.info(f'Best candidate IP for instance {instance_id} is {node_id}')

        if node_id == None:
            logging.error(f'Unable to choose a proper IP address for this server !')
            return False
    else:
        node_id = instance_id

    # download the new GDB folder from S3
    gdbfolder_fullpath_new = os.path.join(gdb_dest_dir, gdbfolder_new)
    if DEBUG_SKIP_DOWNLOAD_FROM_S3 == False:
        if not os.path.exists(gdbfolder_fullpath_new):
            os.makedirs(gdbfolder_fullpath_new)

        gdb_downloaded_ok = download_from_s3(gdbfolder_fullpath_new, gdb_s3uri_new)
        if gdb_downloaded_ok == False:
            logging.error('Error downloading new GDB from S3, cannot conitnue')
            return False

    if DEBUG_SKIP_BALANCER_HANDLING == False:
        # deregister target host from the Target Group
        if True == deregister_target_from_target_group(target_group_arn, node_id):
            logging.info(f'Deregistered {node_id}')
        else:
            logging.info(f'Error deregistering {node_id}')
            return False

    # Stop (ESRI server) service in target host (i.e: this machine)
    service_stopped = False
    logging.info('Stopping ESRI Server service')
    service_stopped = stop_service( service_name )
    if service_stopped == True:
        logging.info('Stopped  ESRI Server service')
    else:
        logging.warning('Failed to stop ESRI Server service')

    service_started_again = False
    if service_stopped == True:

        gdbfolder_fullpath_old = os.path.join(gdb_dest_dir, gdbfolder_old)

        now_timestamp = datetime.datetime.now().strftime('%y%d%m%H%M%S')
        old_root, old_ext = os.path.splitext(gdbfolder_fullpath_old)
        gdbfolder_fullpath_moved_out_prod = old_root + '-' + now_timestamp + old_ext

        # rename current (old) GDB to suffix _(datetime)
        if os.path.exists(gdbfolder_fullpath_old):
            logging.info(f'Renaming current GDB folder {gdbfolder_fullpath_old} to {gdbfolder_fullpath_moved_out_prod}')
            os.rename(gdbfolder_fullpath_old, gdbfolder_fullpath_moved_out_prod)
        else:
            logging.warning(f'Skipping renaming of folder because missing: {gdbfolder_fullpath_old}')

        # rename new GDB to production GDB (the 'old' name)
        logging.info(f'Renaming new     GDB folder {gdbfolder_fullpath_new} to {gdbfolder_fullpath_old}')
        os.rename(gdbfolder_fullpath_new, gdbfolder_fullpath_old)

    # Start (ESRI server) service in target host (i.e: this machine).
    # (it is safer to do it even if stopping the service reported it was not stopped).
    logging.info(f'Starting ESRI Server service')
    service_started_again = start_service( service_name )
    logging.info(f'Started  ESRI Server service')

    if DEBUG_SKIP_BALANCER_HANDLING == False:
        # register again target host into Target Group
        if True == register_target_into_target_group(target_group_arn, node_id):
            logging.info(f'Registered {node_id}')
        else:
            logging.info(f'Error registering {node_id}')
            return False

    return (service_stopped and service_started_again)


def get_loadbalancer_arn(loadbalancer_name):
    """Gets from AWS the ARN of the load balancer by its name.
    ARN is the unique identifier we can use in virtually any call for any resource."""

    expectedSyntax = False

    try:
        resp = elbv2.describe_load_balancers(Names=[ loadbalancer_name ])
    except elbv2.exceptions.LoadBalancerNotFoundException:
        logging.error(f'Wrong load balancer name {loadbalancer_name}')
        return False,''

    loadbalancers_list_tag = 'LoadBalancers'

    if loadbalancers_list_tag in resp:
        if isinstance( resp[ loadbalancers_list_tag ], list ):
            if len( resp[ loadbalancers_list_tag ] ) == 1:
                if 'LoadBalancerName' in resp[loadbalancers_list_tag][0]:
                    if 'LoadBalancerArn' in resp[loadbalancers_list_tag][0]:
                        expectedSyntax = True

    if expectedSyntax == False:
        logging.error('Error in syntax describing load balancerS (note the S)')
        return False, ''

    loadbalancer_name_retrieved = resp[loadbalancers_list_tag][0]['LoadBalancerName']
    loadbalancer_arn = resp[loadbalancers_list_tag][0]['LoadBalancerArn']

    if loadbalancer_name_retrieved != loadbalancer_name:
        logging.error('Strange mismatch in loadbalancer name, aborting')
        return False, ''

    logging.info(f'Retrieved ARN for loadbalancer {loadbalancer_name}, ARN: {loadbalancer_arn}')

    return True, loadbalancer_arn


def get_target_group_info(loadbalancer_arn):
    """Gets from AWS the ARN of the target group the ELB redirects traffic to, and the Target Type.
    We enforce a single TG in the NLB, fail if more than one reported.

    :returns: query was Ok, target_group_arn, target_type_is_IP
    :rtype: bool, string, bool
    """

    expectedSyntax = False
    target_type_is_IP = False

    try:
        resp = elbv2.describe_target_groups(LoadBalancerArn=loadbalancer_arn)
    except elbv2.exceptions.LoadBalancerNotFoundException:
        logging.error(f'Wrong load balancer name {loadbalancer_name}')
        return False,'',target_type_is_IP

    target_groups_list_tag = 'TargetGroups'

    if target_groups_list_tag in resp:
        if isinstance( resp[target_groups_list_tag], list ):
            if len( resp[target_groups_list_tag] ) == 1:
                if 'TargetGroupName' in resp[target_groups_list_tag][0]:
                    if 'TargetGroupArn' in resp[target_groups_list_tag][0]:
                        if 'TargetType' in resp[target_groups_list_tag][0]:
                            expectedSyntax = True
            else:
                logging.error(f'More than one Target Group reported for loadbalancer of ARN: {loadbalancer_arn}')

    if expectedSyntax == False:
        logging.error('Error in syntax describing Target GroupS (note the S)')
        return False, '', target_type_is_IP

    target_group_name= resp[target_groups_list_tag][0]['TargetGroupName']
    target_group_arn = resp[target_groups_list_tag][0]['TargetGroupArn' ]
    target_type_is_IP= resp[target_groups_list_tag][0]['TargetType'].lower() == 'ip'

    logging.info(f'Retrieved ARN for Target Group of the balancer; TG name: {target_group_name}, ARN: {target_group_arn}')

    return True, target_group_arn, target_type_is_IP


def get_target_hosts(target_group_arn):
    """Gets from AWS the list of target hosts given the target group ARN

    :returns: query was Ok, list of targets
    :rtype: bool, string
    """

    expectedSyntax = False

    try:
        resp = elbv2.describe_target_health(TargetGroupArn=target_group_arn)
    except elbv2.exceptions.TargetGroupNotFoundException:
        logging.error(f'Wrong Target Group of ARN {target_group_arn}')
        return False,''

    target_health_descr_list_tag = 'TargetHealthDescriptions'
    num_targets = 0

    if target_health_descr_list_tag in resp:
        if isinstance( resp[target_health_descr_list_tag], list ):
            num_targets = len( resp[target_health_descr_list_tag] )
            if num_targets >= 1:
                if 'Target' in resp[target_health_descr_list_tag][0]:
                    if 'Id' in resp[target_health_descr_list_tag][0]['Target']:
                        expectedSyntax = True
            else:
                logging.error(f'No host defined in Target Group ARN: {target_group_arn}')

    if expectedSyntax == False:
        logging.error('Error in syntax describing Target Group Health, could not retrieve target hosts')
        return False, ''

    # build the target host list going through the response
    target_hosts = list()
    for target_health_descr in resp[target_health_descr_list_tag]:
        # assume response is well built
        if 'Target' in target_health_descr:
            if 'Id' in target_health_descr['Target']:
                target_hosts.append( target_health_descr['Target']['Id'] )

    return True, target_hosts


def get_target_health(target_group_arn, node_id):
    """Gets from AWS the health status (you can say 'registering status') of a target in the target group ARN

    :returns: query was Ok, target status
    :rtype: bool, string
    """

    expectedSyntax = False

    req_targets = list()
    req_targets.append( dict() )
    req_targets[0]['Id'] = node_id

    try:
        resp = elbv2.describe_target_health(TargetGroupArn=target_group_arn, Targets=req_targets)
    except elbv2.exceptions.InvalidTargetException:
        logging.error(f'Invalid target {node_id} trying to get target health')
        return False,''
    except elbv2.exceptions.TargetGroupNotFoundException:
        logging.error(f'Wrong Target Group of ARN {target_group_arn}')
        return False,''
    except: return False,''

    target_health_descr_list_tag = 'TargetHealthDescriptions'
    num_targets = 0

    # https://boto3.amazonaws.com/v1/documentation/api/1.9.42/reference/services/elbv2.html#ElasticLoadBalancingv2.Client.describe_target_health
    if target_health_descr_list_tag in resp:
        if isinstance( resp[target_health_descr_list_tag], list ):
            num_targets = len( resp[target_health_descr_list_tag] )
            if num_targets == 1:
                if 'Target' in resp[target_health_descr_list_tag][0]:
                    if 'Id' in resp[target_health_descr_list_tag][0]['Target']:
                        if node_id == resp[target_health_descr_list_tag][0]['Target']['Id']:
                            if 'TargetHealth' in resp[target_health_descr_list_tag][0]:
                                if 'State' in resp[target_health_descr_list_tag][0]['TargetHealth']:
                                    expectedSyntax = True

    if expectedSyntax == False:
        logging.error(f'Error in syntax describing Target Group Health when querying for node {node_id}')
        return False, ''

    # get the value we're interested in
    health_status = resp[target_health_descr_list_tag][0]['TargetHealth']['State']

    return True, health_status


def get_best_candidate_ip_address(instance_id, target_hosts):
    """Returns the best candidate address to be the IP of this host to be used in the Target Group.

    - If only 1 (IPv4) address is avaialable, it is the best candidate.
    - If more than 1 (IPv4) address are avaialable
      - if at least one is in the list of peers of the Target Group (ideally just 1), any will make it.
      - if none is in the list, then just take any IP addre, we have no clues.

    :returns: query was Ok, ip_addr or None if none is good enough (or none found)
    :rtype: bool, str
    """

    best_ip_addr = None
    ip_labelled_as_Primary = None

    ec2 = session.resource('ec2')
    ec2inst = None
    try:
        ec2inst = ec2.Instance(instance_id)
    except:
        logging.error(f'Error describing instance {instance_id}')
        return False, best_ip_addr

    # build the list of all (private) IPs assigned to the instance
    host_ip_addrs = list()
    for network_interface in ec2inst.network_interfaces_attribute:
        for private_address in network_interface['PrivateIpAddresses']:
            host_ip_addrs.append( private_address['PrivateIpAddress'] )
            if private_address['Primary'] == True:
                if ip_labelled_as_Primary == None: ip_labelled_as_Primary = private_address['PrivateIpAddress']

    logging.debug(f'IP addresses got for instance {instance_id}: {host_ip_addrs}')

    # now, business logic:
    num_ip_addrs = len( host_ip_addrs )

    if num_ip_addrs == 0:
        return False, best_ip_addr

    if num_ip_addrs == 1:
        best_ip_addr = host_ip_addrs[0]
        return True, best_ip_addr

    # More than one address, take first which exists in TargetGroup list of targets
    for candidate_ip in host_ip_addrs:
        if candidate_ip in target_hosts:
            best_ip_addr = candidate_ip
            return True, best_ip_addr
    
    # Finally, well, we have several IPs but none is in the TargetGroup.
    # Take Primary, or, if none, then any.
    if ip_labelled_as_Primary != None:
        best_ip_addr = ip_labelled_as_Primary
    else:
        best_ip_addr = host_ip_addrs[0]

    return True, best_ip_addr


def download_from_s3(dst_dir, src_s3uri):
    """Downloads a file from S3 to the destination full path

    :param str dst_dir:   full path of the destination (including filename)
    :param str src_s3uri: S3 uri of the source
    """

    # parse a little bit S3 uri
    uri_fragments = src_s3uri.split('/', 3)
    bucket_str = uri_fragments[2]
    key_str = uri_fragments[3]
    logging.debug(f'Using bucket {bucket_str}')
    logging.debug(f'Using key    {key_str}')

    s3 = session.resource('s3')
    bucket = s3.Bucket(bucket_str)
    for obj in bucket.objects.filter(Prefix=key_str):
        target = os.path.join(dst_dir, os.path.relpath(obj.key, key_str))
        if not os.path.exists(os.path.dirname(target)):
            os.makedirs(os.path.dirname(target))
        if obj.key[-1] == '/':
            continue

        try:
            bucket.download_file(obj.key, target)
        except:
            logging.error(f'Error downloading file from {obj.key} from {src_s3uri}')
            return False

    return True


def deregister_target_from_target_group(target_group_arn, node_id):
    """Deregister the node from the target group.
    It verifies first whether it is previously registered, and after if it was actually deregistered.

    Note it takes minutes for the target to be completely forgotten by the load balancer.
    node_id can be an instance_id or an IP address, depending on TargetType of the TargetGroup.
    """

    logging.debug(f'Deregistering {node_id}')

    # get targets registered in the target group
    ret, target_hosts = get_target_hosts( target_group_arn )
    if ret == False: return False

    # if target is not registered, return on success
    if not ( node_id in target_hosts ): return True

    # command the deregistering
    req_targets = list()
    req_targets.append( dict() )
    req_targets[0]['Id'] = node_id

    try:
        resp = elbv2.deregister_targets(TargetGroupArn=target_group_arn, Targets=req_targets)
    except elbv2.exceptions.InvalidTargetException:
        logging.error(f'Invalid target {node_id} trying to deregister')
        return False
    except elbv2.exceptions.TargetGroupNotFoundException:
        logging.error(f'Wrong Target Group {target_group_arn} trying to deregister')
        return False

    # Get targets registered again and make sure the selected one is not registered anymore
    # It takes a little bit, retry querying some times and allow some time between queries

    deregister_start = time.time()

    target_hosts_after = None           # just to keep it in scope
    current_try = 0
    while current_try < WAIT_TARGET_DEREGISTERED_CHECK_MAX_NUM_TRIES:

        logging.debug(f'Try {current_try}, waiting {WAIT_TARGET_DEREGISTERED_CHECK_SECS} secs to check whether target is deregistered')
        time.sleep(WAIT_TARGET_DEREGISTERED_CHECK_SECS)

        logging.debug('Querying target hosts in the Target Group')
        ret_after, target_hosts_after = get_target_hosts( target_group_arn )

        # Note the status of the deregistering is also informed, but we ignore it for now.
        # We could also ask only for our target status, what would be fine, too.
        if ret_after == False:
            logging.warning(f'Try {current_try}, error retrieving the targets AFTER deregistering host {node_id}')
        else:
            logging.debug(f"Try {current_try}, retrieved list of target hosts AFTER deregistering is: {' '.join(target_hosts_after)}")

        if node_id in target_hosts_after:
            logging.info(f'Target {node_id} already deregistered, but still in list')
        else: break

        current_try += 1

    deregister_end = time.time()
    deregister_elapsed_secs = deregister_end - deregister_start
    logging.debug(f'We were {deregister_elapsed_secs:4.1f} secs waiting for target deregistration')

    # verify whether the target is still in list after all the required retries (maybe we broke the loop)
    if node_id in target_hosts_after:
        logging.error(f'Error deregistering target {node_id}, still registered in Target Group')
        return False

    logging.debug(f'Deregistered  {node_id}')

    return True


def register_target_into_target_group(target_group_arn, node_id):
    """Register the instance into the target group.
    It verifies first it was not previously registered, and after if it gets actually registered.

    Note it takes minutes for the load balancer to complete the addition of the new target (to steady state).
    node_id can be an instance_id or an IP address, depending on TargetType of the TargetGroup.
    """

    logging.debug(f'Registering {node_id}')

    # get targets registered in the target group
    ret, target_hosts = get_target_hosts( target_group_arn )
    if ret == False: return False

    # If target is already registered, return on success.
    # Note this is only the first time, before trying to register. Later on we HAVE TO wait till
    #   target is Ready, healthy or whatever.
    if node_id in target_hosts: return True

    health_transient_states= [ 'initial', 'draining', 'unavailable' ]
    health_final_ok_states = [ 'healthy' ]

    # command the registering
    req_targets = list()
    req_targets.append( dict() )
    req_targets[0]['Id'] = node_id

    try:
        resp = elbv2.register_targets(TargetGroupArn=target_group_arn, Targets=req_targets)
    except elbv2.exceptions.InvalidTargetException:
        logging.error(f'Invalid target {node_id} trying to deregister')
        return False
    except elbv2.exceptions.TargetGroupNotFoundException:
        logging.error(f'Wrong Target Group {target_group_arn} trying to deregister')
        return False
    except: return False

    # Ask for the registration status of the new target, and do not continue till it is completely added
    # It takes a little bit, retry querying some times and allow some time between queries

    register_start = time.time()

    target_health = None        # just to keep it in scope
    current_try = 0
    while current_try < WAIT_TARGET_REGISTERED_CHECK_MAX_NUM_TRIES:

        logging.debug(f'Try {current_try}, waiting {WAIT_TARGET_REGISTERED_CHECK_SECS} secs to check whether target is registered')
        time.sleep(WAIT_TARGET_REGISTERED_CHECK_SECS)

        logging.debug(f'Querying status of {node_id} registering in the Target Group')

        ret_after, target_health = get_target_health( target_group_arn, node_id )

        # Note the status of the deregistering is also informed, but we ignore it for now.
        # We could also ask only for our target status, what would be fine, too.
        if ret_after == False:
            logging.warning(f'Try {current_try}, error retrieving targets health after registering host {node_id}')
        else:
            logging.debug(f"Try {current_try}, target health after registering it is: {target_health}")

        if target_health in health_transient_states:
            logging.info(f'Target {node_id} registration commanded, but still in state {target_health}')
        else: break

        current_try += 1

    register_end = time.time()
    register_elapsed_secs = register_end - register_start
    logging.debug(f'We were {register_elapsed_secs:4.1f} secs waiting for target registration')

    # verify whether the target is still in list after all the required retries (maybe we broke the loop)
    if not (target_health in health_final_ok_states):
        logging.error(f'Error registering target {node_id}, current Health state is {target_health}')
        return False

    logging.debug(f'Registered  {node_id}')

    return True


def stop_service(service_name):
    """Stops the required service running in the local host.

    Proper admin rights have to be assigned to the user running the script."""

    scvType, svcState, svcControls, err, svcErr, svcCP, svcWH = win32serviceutil.QueryServiceStatus(service_name)

    if svcState == win32service.SERVICE_RUNNING:
        logging.debug(f'Service {service_name} is stopping ...')

        win32serviceutil.StopService(service_name)

        time.sleep(0.3)         # a fixed wait to avoid querying immediately
        wait_service_state(service_name, win32service.SERVICE_STOPPED, "STOPPED",
                            WAIT_SERVICE_STOPPED_CHECK_SECS, WAIT_SERVICE_STOPPED_CHECK_MAX_NUM_TRIES) 

        logging.debug(f'Service {service_name} is stopped.')

    svcState = win32serviceutil.QueryServiceStatus(service_name)[1]
    if svcState == win32service.SERVICE_STOPPED:
        logging.debug(f'Service {service_name} is stopped.')
    else:
        logging.debug(f'Service {service_name} failed to get STOPPED after wait.')
 
    return (svcState == win32service.SERVICE_STOPPED)


def start_service(service_name):
    """Starts the required service running in the local host.

    Proper admin rights have to be assigned to the user running the script."""

    scvType, svcState, svcControls, err, svcErr, svcCP, svcWH = win32serviceutil.QueryServiceStatus(service_name)

    if svcState == win32service.SERVICE_STOPPED:
        logging.debug(f'Service {service_name} is starting ...')

        win32serviceutil.StartService(service_name)

        time.sleep(0.3)         # a fixed wait to avoid querying immediately
        wait_service_state(service_name, win32service.SERVICE_RUNNING, "RUNNING",
                            WAIT_SERVICE_RUNNING_CHECK_SECS, WAIT_SERVICE_RUNNING_CHECK_MAX_NUM_TRIES) 

    svcState = win32serviceutil.QueryServiceStatus(service_name)[1]
    if svcState == win32service.SERVICE_RUNNING:
        logging.debug(f'Service {service_name} is running.')
    else:
        logging.debug(f'Service {service_name} failed to get RUNNING after wait.')
 
    return (svcState == win32service.SERVICE_RUNNING)


def wait_service_state(service_name, target_service_state, target_service_state_name, wait_check_secs, wait_check_max_num_tries):
    """Waits till service state switches to the target status, with a timeout"""

    wait_time_start = time.time()
    current_try = 0
    while current_try < wait_check_max_num_tries:

        svcState = win32serviceutil.QueryServiceStatus(service_name)[1]

        if svcState != target_service_state:
            logging.info(f'Service action commanded, but still in waiting it gets {target_service_state_name}')
        else: break

        logging.debug(f'Try {current_try}, waiting {wait_check_secs} secs to check whether service switches to the new status')
        time.sleep(wait_check_secs)

        current_try += 1

    wait_time_end = time.time()
    wait_elapsed_secs = wait_time_end - wait_time_start
    logging.debug(f'We were {wait_elapsed_secs:4.1f} secs waiting for service status change')

    return True


def validate_arguments(src_s3uri):
    '''Validates the argument's syntax

    :param str src_s3uri: S3 uri of the new file to download
    '''

    # src_s3uri has to be an S3 URI
    if (src_s3uri.lower().startswith('s3://') == False) or (src_s3uri.count('/') < 3):
        logging.error(f'Not a proper S3 URI: {src_s3uri}')
        return False

    return True


def main():
    global session, elbv2

    parser = argparse.ArgumentParser(description='Updates GDB file in production servers')
    parser.add_argument('--ver', help='Prints the version', action='store_true')
    parser.add_argument('--aws_profile', help='AWS profile to use for the commands', default='PublishEsriDatasetServer')
    parser.add_argument('--loadbalancer_name', help='Name of the AWS NLB giving access to ESRI servers',
                        default='arcgis-prod-server-nlb')
    parser.add_argument('--service_name', help='Name of the ESRI service to be restarted', default='ArcGIS Server')
    parser.add_argument('--gdb_folder_old', help='Old (currently in use in PROD) GDB folder (only the folder name)',
                        required = False)
    #parser.add_argument('--gdb_folder_new', help='New GDB folder (only the folder name)',
    #                    default='master_new.gdb')
    parser.add_argument('--gdb_server_directory', help='Local host directory where the GDB folders are located',
                        required = False)
    parser.add_argument('--gdb_s3uri_new', help='Complete S3 Uri to download the new GDB folder from', required = False)
    parser.add_argument('--esri_server_loglevel', help='Log level', default='INFO')
    parser.add_argument('--host_instance', help='Host instance number', default='1')

    args, unknown = parser.parse_known_args()

    if DEBUG_REMOVE_QUOTES_FROM_ARGS == True:
        for arg in vars(args):
            value = getattr(args, arg)
            if isinstance(value, str):
                setattr(args, arg, value.replace('\'', ''))

    if args.ver:
        print('farm-tool 1')
        print(f'PublishEsriDatasetServer{args.host_instance} 1.0')
        return OK_EXIT

    logging.basicConfig( level=logging._nameToLevel[ args.esri_server_loglevel ] )

    # check args
    if validate_arguments(args.gdb_s3uri_new) == False:
        logging.critical('Wrong arguments')
        return ERROR_EXIT

    # args generated automatically from user-provided args
    gdb_folder_new = args.gdb_folder_old + '-new'

    # Init boto3 resources to be used urbi et orbi
    session = None
    if args.aws_profile == 'DONT_FORCE_ANY': session = boto3.session.Session()
    else: session = boto3.session.Session(profile_name=args.aws_profile)
    #avlb_res = session.get_available_resources()
    #avlb_srv = session.get_available_services()
    elbv2 = session.client('elbv2')

    op_result = update_this_server_gdb( args.loadbalancer_name, args.service_name,
                                        args.gdb_folder_old, gdb_folder_new,
                                        args.gdb_server_directory, args.gdb_s3uri_new )
    if op_result == False:
        logging.error('Failed to update GDB folder in this server')
        return ERROR_EXIT

    logging.info('GDB folder successfully updated in this server')

    return OK_EXIT

if __name__ == '__main__':
    main()

