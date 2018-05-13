#! /usr/bin/perl -w
#
# usage:
#    ged2rss.pl
#
# License: GPL
# michael.aubertin@gmail.com from EyesOfNetwork Team
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

use POSIX qw(locale_h); 
use POSIX qw(strftime); 
use File::Basename;
use XML::Simple;

setlocale(LC_CTYPE, "en_EN");
$XML::Simple::PREFERRED_PARSER = XML::Parser;

use strict;
use Getopt::Long;
use vars qw($opt_V $opt_h $opt_t $verbose $PROGNAME $Revision);
use utils qw(%ERRORS &print_revision &support &usage);
use DBI;


$PROGNAME="ged2rss.pl";

sub print_help ();
sub print_usage ();
sub create_xml_rss_file ($;$);
sub write_event_host_to_rss ($);
sub build_HostList ($);
sub build_ServiceList ($);
sub set_filter_type($;$);


$ENV{'PATH'}='';
$ENV{'BASH_ENV'}='';
$ENV{'ENV'}='';
$Revision="0.2";

my $DB_USER = "gedadmin";
my $DB_PASS = "whaza";

my $EON_Host = `/bin/hostname | /usr/bin/tr '\n' ' ' | /bin/sed -e 's: ::g'`;
my $RSS_Filter_Name = "";
my $RSS_Path="/srv/eyesofnetwork/ged/var/www/";
my $RSS_File="/srv/eyesofnetwork/ged/var/www/rss.xml";
my $FILTER_Path="/srv/eyesofnetwork/eonweb/cache";
my $GetGedFilterList="/bin/ls $FILTER_Path/*-ged.xml 2> /dev/null";
my $db_ged = DBI->connect('DBI:mysql:ged', $DB_USER, $DB_PASS ) || die "Could not connect to database: $DBI::errstr";
my $db_lilac = DBI->connect('DBI:mysql:lilac', $DB_USER, $DB_PASS ) || die "Could not connect to database: $DBI::errstr";
my @FilterListFile;
my @UsersList;
my @HostList;
my @HostTemplate;
my @ServiceList;
my @ServiceTemplate;
my $FileRSS;

sub print_usage () {
    print "Usage:
   $PROGNAME [-v] [-V] [-h]
   $PROGNAME --help
   $PROGNAME --version";
}

sub print_help () {
        print "$PROGNAME Revision $Revision\n \n";
        print "Copyright (c) 2013 Michael Aubertin <michael.aubertin\@gmail.com> Licenced under GPLV2\n";
        
        print_usage();
        print "-v, --verbose  Print some extra debugging information (not advised for normal operation)\n";
}

sub create_xml_rss_file ($;$) {

    my $RSS_Filter_Path = "/srv/eyesofnetwork/ged/var/www/";
    $RSS_Filter_Name = shift;
    
    # Handle here invalid filter name.
    if ( (not defined($RSS_Filter_Name)) || $RSS_Filter_Name eq '' ) {
       $RSS_Filter_Name = 'unset';
       print "create_xml_rss_file -> Sub Create_xml_rss_file: Filter Name NOT DEFINE -> $RSS_Filter_Name \n" if $verbose;
       return 1;
       }
    
    my $User = shift;
    
    if ( defined($User) && $User ne '' && $User ne '0' ) {
       $RSS_Filter_Name = $RSS_Filter_Path . "rss-" . $User . "-" . $RSS_Filter_Name . ".xml";
       }
    else {
           print "create_xml_rss_file -> Sub Create_xml_rss_file: USER Name NOT DEFINE -> $User \n" if $verbose;
         }   
    
    if ( ! open(FileRSS, "> $RSS_Filter_Name" ) ) {
           print "CRITICAL: Cannot open  $RSS_Filter_Name\n";
            exit $ERRORS{'UNKNOWN'};
    }
    
    print "create_xml_rss_file -> Sub Create_xml_rss_file: RSS_Filter_Name -> $RSS_Filter_Name \n" if $verbose;
    return 0;
}

sub write_event_host_to_rss ($) {
   my $Ged_Request = shift;

   while (my $event = $Ged_Request->fetchrow_hashref( 'NAME_lc' )) {
      print "write_event_host_to_rss -> equipment: ", $event->{ equipment }, " service: ", $event->{ service }, " description: ", $event->{ description }, "\n" if $verbose;
      print FileRSS "<item>\n";
      print FileRSS "<title>Event EON id(",$event->{ id },"):", $event->{ equipment }," service:",$event->{ service },"</title>\n";
      print FileRSS "<description>", $event->{ description } ,"</description>\n";

      if ( $event->{ service } eq "HOSTDOWN" || $event->{ service } eq "HOST UNREACHABLE" )
      {
        print FileRSS "<link>http://$EON_Host/thruk/cgi-bin/extinfo.cgi?type=1&amp;host=",$event->{ equipment },"</link>\n";
      }
      else
      {
        print FileRSS "<link>http://$EON_Host/thruk/cgi-bin/extinfo.cgi?type=2&amp;host=",$event->{ equipment },"&amp;service=",$event->{ service },"</link>\n";
      }
      print FileRSS "</item>\n";
   }
}

sub build_HostList ($) {
  my $setval = shift;
  my @HostList;
  my @HostTemplate;

  print "DEBUG: Building Hostlist for $setval (Fct: build_HostList()).\n" if $verbose;
  print "DEBUG: Resquest is: select nagios_host.name from nagios_hostgroup,nagios_host,nagios_hostgroup_membership where nagios_hostgroup.id=nagios_hostgroup_membership.hostgroup and nagios_host.id=nagios_hostgroup_membership.host and nagios_hostgroup.name like \'". $setval ."\';\n" if $verbose;
  my $Lilac_Request = $db_lilac->prepare('select nagios_host.name from nagios_hostgroup,nagios_host,nagios_hostgroup_membership where nagios_hostgroup.id=nagios_hostgroup_membership.hostgroup and nagios_host.id=nagios_hostgroup_membership.host and nagios_hostgroup.name like \''. $setval .'\';');
  $Lilac_Request->execute();

  while ( my $lilac_host_list = $Lilac_Request->fetchrow_hashref( 'NAME_lc')) 
  {
      push(@HostList, $lilac_host_list->{ name });
      print "build_HostList -> Adding host: $lilac_host_list->{ name } \n" if $verbose;
  }

  $Lilac_Request = $db_lilac->prepare('select nagios_host_template.name from nagios_hostgroup,nagios_host_template,nagios_hostgroup_membership where nagios_hostgroup.id=nagios_hostgroup_membership.hostgroup and nagios_host_template.id=nagios_hostgroup_membership.host_template AND nagios_hostgroup.name like \''. $setval .'\';');
  $Lilac_Request->execute();

  while ( my $lilac_host_list = $Lilac_Request->fetchrow_hashref( 'NAME_lc')) 
  {
      push(@HostTemplate, $lilac_host_list->{ name });
      print "build_HostList -> Adding Template: $lilac_host_list->{ name } \n" if $verbose;
  }

  foreach (@HostTemplate) 
  {
      print "build_HostList -> Handeling $_ template \n" if $verbose;
      $Lilac_Request = $db_lilac->prepare('select nagios_host.name from nagios_host,nagios_host_template,nagios_host_template_inheritance where nagios_host.id=nagios_host_template_inheritance.source_host and nagios_host_template.id=nagios_host_template_inheritance.target_template AND nagios_host_template.name like \''. $_ .'\';');
      $Lilac_Request->execute();

      while ( my $lilac_host_list = $Lilac_Request->fetchrow_hashref( 'NAME_lc')) 
      {
          push(@HostList, $lilac_host_list->{ name });
          print "build_HostList -> Adding host: $lilac_host_list->{ name } \n" if $verbose;
      }
  }
  if ( not @HostList ) 
    {
      push(@HostList,"List_Vide");
    }
  
  return @HostList;
}

sub build_ServiceList ($) {
  my $setval = shift;
  my @ServiceList;
  my @ServiceTemplate;

  print "DEBUG: Building ServiceList for $setval (Fct: build_ServiceList()).\n";

  my $Lilac_Request = $db_lilac->prepare('select nagios_service_group.name as servicegroup,nagios_service_group.alias,nagios_service.description as name from nagios_service_group,nagios_service,nagios_service_group_member where nagios_service_group.id=nagios_service_group_member.service_group and nagios_service.id=nagios_service_group_member.service and  nagios_service_group.name like \''. $setval .'\';');
  $Lilac_Request->execute();

  while ( my $lilac_service_list = $Lilac_Request->fetchrow_hashref( 'NAME_lc')) 
  {
      push(@ServiceList, $lilac_service_list->{ name });
      print "build_ServiceList -> Adding Service: $lilac_service_list->{ name } \n" if $verbose;
  }

  $Lilac_Request = $db_lilac->prepare('select nagios_service_group.name,nagios_service_group.alias as servicegroup,nagios_service_template.description as name from nagios_service_group,nagios_service_template,nagios_service_group_member where nagios_service_group.id=nagios_service_group_member.service_group and nagios_service_template.id=nagios_service_group_member.template and nagios_service_group.name like \''. $setval .'\';');
  $Lilac_Request->execute();

  while ( my $lilac_service_list = $Lilac_Request->fetchrow_hashref( 'NAME_lc')) 
  {
      push(@ServiceTemplate, $lilac_service_list->{ name });
      print "build_ServiceList -> Adding Template: $lilac_service_list->{ name } \n" if $verbose;
  }

  foreach (@ServiceTemplate) 
  {
      print "build_ServiceList -> Handeling $_ template \n" if $verbose;
      $Lilac_Request = $db_lilac->prepare('select nagios_service.name from nagios_service,nagios_service_template,nagios_service_template_inheritance where nagios_service.id=nagios_service_template_inheritance.source_service and nagios_service_template.id=nagios_service_template_inheritance.target_template AND nagios_service_template.name like \''. $_ .'\';');
      $Lilac_Request->execute();

      while ( my $lilac_service_list = $Lilac_Request->fetchrow_hashref( 'NAME_lc')) 
       {
          push(@ServiceList, $lilac_service_list->{ name });
          print "build_ServiceList -> Adding host: $lilac_service_list->{ name } \n" if $verbose;
      }
  }
  if ( not @ServiceList ) 
    {
      push(@ServiceList,"List_Vide");
    }
  
  return @ServiceList;
}

sub set_filter_type($;$) {
    
    my $type = shift;
    my $setval = shift;
    my $MainType;

    print "set_filter_type -> Handeling $type,$setval. \n" if $verbose;

    if ( ( $type eq '' ) && ( $setval eq '' )) # TYPE IS SET and value is not empty
    {
        print "set_filter_type -> This filter syntaxt is not valid. \n" if $verbose;
        $MainType = "invalid";
    }
    else # Type is valid, so what type is it ? (empty is considered as description)
    { 
        print "set_filter_type -> ".$type.",".$setval." \n" if $verbose;
        if ( $type eq 'equipment' || $type eq 'hostgroups' ) 
        {
            $MainType="host";
            print "set_filter_type -> MainType = host. \n" if $verbose;
        }
        if ( $type eq 'service' || $type eq 'servicegroups' ) 
        {
            $MainType="service";
            print "set_filter_type -> MainType = Service. \n" if $verbose;
        }
        if ( $MainType eq '' ) 
        {
            $MainType = "description";
            print "set_filter_type -> MainType = description. \n" if $verbose;
        }
    }
    return $MainType;
}

Getopt::Long::Configure('bundling');
GetOptions
        ("V"     => \$opt_V,           "version"      => \$opt_V,
         "h"     => \$opt_h,           "help"         => \$opt_h,
         "v"     => \$verbose,         "verbose"      => \$verbose);

if ($opt_V) {
        print "$PROGNAME Revision: $Revision\n";
        exit $ERRORS{'OK'};
}

$opt_t = $utils::TIMEOUT ;      # default timeout

if ($opt_h) {print_help(); exit $ERRORS{'OK'};}

my $gedq = "/srv/eyesofnetwork/ged/bin/gedq" ;

unless (-x $gedq ) {
        print "Cannot find \"$gedq\"\n";
        exit $ERRORS{'UNKNOWN'};
}

$SIG{'ALRM'} = sub {
        print "Timeout: No Answer from $gedq\n";
        exit $ERRORS{'UNKNOWN'};
};

alarm($opt_t);


####################################################################
#
# int main();
#
#

if ( ! open(CMDLS,"$GetGedFilterList |") ) 
{
        print "CRITICAL: Cannot execute gently $GetGedFilterList\n";
        exit $ERRORS{'UNKNOWN'};
}
 
while ( <CMDLS> ) 
{
   push(@FilterListFile, "$_");
   print "Debug2: $_" if $verbose;
}
close CMDLS;

# Main stream
if ( ! open(FileRSS, ">$RSS_File.tmp" ) ) 
{
        print "CRITICAL: Cannot open  $RSS_File.tmp\n";
        exit $ERRORS{'UNKNOWN'};
}

print "Debug: $RSS_File.tmp\n" if $verbose;
print "      Debug2: <?xml version= \"1.0\"?> <rss version= \"2.0\"> <channel> \n" if $verbose;

print FileRSS "<?xml version= \"1.0\"?> <rss version= \"2.0\"> <channel> \n";

if ( ! open(GED2XML, ">/tmp/ged2xml.tmp" ) ) {
        print "CRITICAL: Cannot open  /tmp/ged2xml.tmp\n";
        exit $ERRORS{'UNKNOWN'};
}
close GED2XML;

my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active;');
$Ged_Request->execute();

write_event_host_to_rss($Ged_Request);

my $MoveFinalGlobalRSS="/bin/mv -f $RSS_File.tmp $RSS_File";

if ( ! open(CMDMV,"$MoveFinalGlobalRSS |") ) {
        print "CRITICAL: Cannot move gently $MoveFinalGlobalRSS\n";
        exit $ERRORS{'UNKNOWN'};
}
close CMDMV;

print FileRSS "</channel> </rss>\n";
close FileRSS;

###### End GLOBAL RSS ############

###### Ok, let construct one RSS per user and per filter

foreach my $CurrentFilter(@FilterListFile) # Building User List from filter present on the system
{
        print "Main -> ", $CurrentFilter, " \n" if $verbose;
        @_ = split ('/',$CurrentFilter);
        my $CurrentFilter_File = $_[5];
        print "Main -> CurrentFilter_File is ->", $CurrentFilter_File, " \n" if $verbose;
        @_ = split ("-ged.xml",$CurrentFilter_File);
        my $Current_User = $_[0];
        push(@UsersList, $Current_User);
        print "Main -> ", $Current_User, " \n" if $verbose;
}

foreach my $CurrentUser(@UsersList) # Loop on each User
{
        use XML::Simple;
        use Data::Dumper;
        my $xml2 = new XML::Simple (KeyAttr=>'filter');
        my $MainType='';
        
        my $CurrentFilterXMLFile = $xml2->XMLin("/srv/eyesofnetwork/eonweb/cache/".$CurrentUser."-ged.xml");
        
        print "Main -> /srv/eyesofnetwork/eonweb/cache/".$CurrentUser."-ged.xml \n" if $verbose;
        
        my $e;
        my $filter;
        
        my @host_list;  # Tableau de concatenation des hosts des filtres
        my @service_list; # Tableau de concatenation des services des filtres
        
        if(ref($CurrentFilterXMLFile->{filters}) eq 'ARRAY') 
        { # Multi filter user.
                foreach $e (@{$CurrentFilterXMLFile->{filters}})
                        {
                                          my $val=$e->{name};
                                          print "      Debug2: Filter:".$val." \n" if $verbose;
                                                           
                                          if(ref($e->{filter}) eq 'ARRAY') # Multi-Filter user, and multi-condition filter.
                                          {
                                               # CREATE XML RSS
                                               print "DEBUG:  This have got several filter, and the current filter avec several conditions.\n" if $verbose;
                                               create_xml_rss_file ($val,$CurrentUser);
                                               print FileRSS "<?xml version= \"1.0\"?> <rss version= \"2.0\"> <channel> \n";
                                               foreach $filter (@{$e->{filter}}) {
                                                    my $type = $filter->{name};
                                                    my $setval = $filter->{content};
                                                    
                                                    if ( defined($type) && defined($setval) )
                                                    {
                                                      $MainType=set_filter_type($type,$setval);
                                                    }
                                                    else
                                                    {
                                                      print "Main -> Filter type or content not defined. INVALID filter. \n" if $verbose;
                                                      $MainType = "invalid";
                                                    }

                                                    # ACT Now
                                                    if ( $MainType eq 'host' && ( not $MainType eq 'invalid'))
                                                    {
                                                      print "DEBUG: Entering multi-filter multi-user. Current type familly: host ($setval) \n" if $verbose;
                                                      if ( defined($type) && $type eq 'equipment' ) # Is this an equipement ?
                                                      {
                                                            print "Main -> Entering multi-filter multi-user. Current type : equipment ($setval)\n" if $verbose;                                                          
                                                            my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where equipment like \''. $setval .'\' ;');
                                                            $Ged_Request->execute();

                                                            write_event_host_to_rss($Ged_Request);
                                                      }
                                                      else ###### At this point, if this is not an equipement filter, it can only be hostgroup.  
                                                      {
                                                        print "DEBUG: Entering multi-filter multi-user. Current type : supposed to be hostgroup  ($setval) \n" if $verbose;
                                                        # I guess i have to build host list :)
                                                        @HostList = (@HostList,build_HostList($setval));                                                     
                                                        
                                                        ##### Now XML to RSS File
                                                        my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where equipment like \''. $HostList[0] .'\' ';
                                                        foreach (@HostList) 
                                                        {
                                                          print "Main ->          HOST: $_ \n" if $verbose;
                                                          $Ged_Filtered_Request = $Ged_Filtered_Request .' OR equipment like \''. $_ .'\' ';
                                                        }
                                                        $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                                                        print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;

                                                        my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                                                        $Ged_Request->execute();

                                                        write_event_host_to_rss($Ged_Request);
                                                        $#HostList = -1;
                                                        $#HostTemplate = -1;
                                                      }
                                                    }

                                                    if ( $MainType eq 'service' && ( not $MainType eq 'invalid'))
                                                    {
                                                      print "DEBUG: Entering multi-filter multi-user. Current type familly: Service ($setval) \n" if $verbose;
                                                      if ( defined($type) && $type eq 'service' ) # Is this an equipement ?
                                                      {
                                                            print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                                                            my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where service like \''. $setval .'\' ;');
                                                            $Ged_Request->execute();

                                                            write_event_host_to_rss($Ged_Request);
                                                      }
                                                      else ###### At this point, if this is not an service filter, it can only be servicegroup.  
                                                      {
                                                        print "DEBUG: Entering multi-filter multi-user. Current type : supposed to be servicegroup  ($setval) \n" if $verbose;
                                                        # I guess i have to build host list :)
                                                        @ServiceList = (@ServiceList,build_HostList($setval));                                                     
                                                        
                                                        ##### Now XML to RSS File
                                                        my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where service like \''. $ServiceList[0] .'\' ';
                                                        foreach (@ServiceList) 
                                                        {
                                                          print "Main ->          SERVICE: $_ \n" if $verbose;
                                                          $Ged_Filtered_Request = $Ged_Filtered_Request .' OR service like \''. $_ .'\' ';
                                                        }
                                                        $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                                                        print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;

                                                        my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                                                        $Ged_Request->execute();

                                                        write_event_host_to_rss($Ged_Request);
                                                      }
                                                    }

                                                    if ( $MainType eq 'description' )
                                                    {
                                                      print "DEBUG: Entering multi-filter multi-user. Current type familly: Description ($setval) \n" if $verbose;
                                                      print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                                                      my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where description like \''. $setval .'\' ;');
                                                      $Ged_Request->execute();

                                                      write_event_host_to_rss($Ged_Request);
                                                    }

                                                } # End foreach filter of this user
                                                print FileRSS "</channel> </rss> \n";
                                                close FileRSS;
                                          }
                                          else # Multi-Filter user, and mono-condition filter.
                                          {
                                              my $type = $e->{filter}->{name};
                                              my $setval = $e->{filter}->{content};
                                              print "DEBUG:  Multi-Filter user, and mono-condition filter.\n" if $verbose;

                                              if ( defined($type) && defined($setval) )
                                              {
                                                  $MainType=set_filter_type($type,$setval);
                                              }
                                              else
                                              {
                                                  print "Main -> Filter type or content not defined. INVALID filter. \n" if $verbose;
                                                  $MainType = "invalid";
                                              }

                                              create_xml_rss_file ($val,$CurrentUser);
                                              print FileRSS "<?xml version= \"1.0\"?> <rss version= \"2.0\"> <channel> \n";
                                              
                                              if ( $MainType eq 'host' && ( not $MainType eq 'invalid'))
                                              {
                                                if ( defined($type) && $type eq 'equipment' ) 
                                                {
                                                  my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where equipment like \''. $setval .'\' ;');
                                                  $Ged_Request->execute();
                                                  write_event_host_to_rss($Ged_Request);
                                                }
                                                else ###### At this point, if this is not an equipement filter, it can only be hostgroup.
                                                {
                                                  # I guess i have to build host list :)
                                                  @HostList = (@HostList,build_HostList($setval));

                                                  ##### Now XML to RSS File
                                                  my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where equipment like \''. $HostList[0] .'\' ';
                                                  foreach (@HostList) 
                                                  {
                                                      print "Main ->          HOST: $_ \n" if $verbose;
                                                      $Ged_Filtered_Request = $Ged_Filtered_Request .' OR equipment like \''. $_ .'\' ';
                                                  }
                                                  $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                                                  print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;
                                                  my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                                                  $Ged_Request->execute();

                                                  write_event_host_to_rss($Ged_Request);
                                                  $#HostList = -1;
                                                  $#HostTemplate = -1;                                                
                                                }  
                                              }

                                              if ( $MainType eq 'service' && ( not $MainType eq 'invalid'))
                                                {
                                                print "DEBUG: Entering multi-filter multi-user. Current type familly: Service ($setval) \n" if $verbose;
                                                if ( defined($type) && $type eq 'service' ) # Is this an equipement ?
                                                  {
                                                    print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                                                    my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where service like \''. $setval .'\' ;');
                                                    $Ged_Request->execute();

                                                    write_event_host_to_rss($Ged_Request);
                                                  }
                                                else ###### At this point, if this is not an service filter, it can only be servicegroup.  
                                                  {
                                                    print "DEBUG: Entering multi-filter multi-user. Current type : supposed to be servicegroup  ($setval) \n" if $verbose;
                                                    # I guess i have to build host list :)
                                                    @ServiceList = (@ServiceList,build_HostList($setval));                                                     
                                                        
                                                    ##### Now XML to RSS File
                                                    my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where service like \''. $ServiceList[0] .'\' ';
                                                    foreach (@ServiceList) 
                                                      {
                                                        print "Main ->          SERVICE: $_ \n" if $verbose;
                                                        $Ged_Filtered_Request = $Ged_Filtered_Request .' OR service like \''. $_ .'\' ';
                                                      }
                                                    $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                                                    print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;

                                                    my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                                                    $Ged_Request->execute();

                                                    write_event_host_to_rss($Ged_Request);
                                                  }
                                                }

                                              if ( $MainType eq 'description' ) 
                                              {
                                                print "DEBUG: Entering multi-filter multi-user. Current type familly: Description ($setval) \n" if $verbose;
                                                print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                                                my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where description like \''. $setval .'\' ;');
                                                $Ged_Request->execute();

                                                write_event_host_to_rss($Ged_Request);
                                              }
                                          print FileRSS "</channel> </rss> \n";
                                          close FileRSS;
                                          }
                         }
        }
        else # Mono Filter user.        
        {
          print "DEBUG: Mono filter user.\n" if $verbose;
          $e=$CurrentFilterXMLFile->{filters};
          my $val=$e->{name};
          if ( ( not defined($val)) || $val eq '' ) 
            {
            print "             Debug4: No more filter defined for this use. \n" if $verbose;
            }
          else 
            {
            print "             Debug4: Filter:".$val." \n" if $verbose;
            }
                                                          
          if(ref($e->{filter}) eq 'ARRAY') # Filtre unique avec plusieur clause de filtrage.
            {
            # CREATE XML RSS
            create_xml_rss_file ($val,$CurrentUser);
            print FileRSS "<?xml version= \"1.0\"?> <rss version= \"2.0\"> <channel> \n";
            foreach $filter (@{$e->{filter}})
              {
              my $type = $filter->{name};
              my $setval = $filter->{content};
                                     
              if ( defined($type) && defined($setval) )
                {
                $MainType=set_filter_type($type,$setval);
                }
              else
                {
                print "Main -> Filter type or content not defined. INVALID filter. \n" if $verbose;
                $MainType = "invalid";
                }

              if ( $MainType eq 'host' && ( not $MainType eq 'invalid'))
                {
                  print "DEBUG: Entering mono filter. Current type familly: host ($setval) \n" if $verbose;
                  if ( defined($type) && $type eq 'equipment' ) # Is this an equipement ?
                    {
                      print "Main -> Entering mono filter. Current type : equipment ($setval)\n" if $verbose;                                                          
                      my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where equipment like \''. $setval .'\' ;');
                      $Ged_Request->execute();
                      write_event_host_to_rss($Ged_Request);
                    }
                  else ###### At this point, if this is not an equipement filter, it can only be hostgroup.  
                    {
                      print "DEBUG: Entering mono filter. Current type : supposed to be hostgroup  ($setval) \n" if $verbose;
                      # I guess i have to build host list :)
                      @HostList = (@HostList,build_HostList($setval));
                      ##### Now XML to RSS File
                      my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where equipment like \''. $HostList[0] .'\' ';
                      foreach (@HostList) 
                        {
                          print "Main ->          HOST: $_ \n" if $verbose;
                          $Ged_Filtered_Request = $Ged_Filtered_Request .' OR equipment like \''. $_ .'\' ';
                        }
                      $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                      print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;
                      my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                      $Ged_Request->execute();
                      write_event_host_to_rss($Ged_Request);
                      $#HostList = -1;
                      $#HostTemplate = -1;
                    }
                }

              if ( $MainType eq 'service' && ( not $MainType eq 'invalid'))
                {
                  print "DEBUG: Entering mono filter. Current type familly: Service ($setval) \n" if $verbose;
                  if ( defined($type) && $type eq 'service' ) # Is this an equipement ?
                    {
                      print "Main -> Entering mono filter. Current type : service ($setval)\n" if $verbose;                                                          
                      my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where service like \''. $setval .'\' ;');
                      $Ged_Request->execute();

                      write_event_host_to_rss($Ged_Request);
                    }
                  else ###### At this point, if this is not an service filter, it can only be servicegroup.  
                    {
                      print "DEBUG: Entering mono filter. Current type : supposed to be servicegroup  ($setval) \n" if $verbose;
                      # I guess i have to build host list :)
                      @ServiceList = (@ServiceList,build_HostList($setval));                                                     
                                                        
                      ##### Now XML to RSS File
                      my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where service like \''. $ServiceList[0] .'\' ';
                      foreach (@ServiceList) 
                        {
                          print "Main ->          SERVICE: $_ \n" if $verbose;
                          $Ged_Filtered_Request = $Ged_Filtered_Request .' OR service like \''. $_ .'\' ';
                        }
                      $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                      print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;
                      my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                      $Ged_Request->execute();

                      write_event_host_to_rss($Ged_Request);
                    }
                }


              if ( $MainType eq 'description' )
                {
                  print "DEBUG: Entering multi-filter multi-user. Current type familly: Description ($setval) \n" if $verbose;
                  print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                  my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where description like \''. $setval .'\' ;');
                  $Ged_Request->execute();

                  write_event_host_to_rss($Ged_Request);
                }

              } # End foreach filter of this user
            print FileRSS "</channel> </rss> \n";
            close FileRSS;
            }
          else # Filtre unique avec une seule clause de filtrage.
            {
              create_xml_rss_file ($val,$CurrentUser);
              print FileRSS "<?xml version= \"1.0\"?> <rss version= \"2.0\"> <channel> \n";
              my $type = $e->{filter}->{name};
              my $setval = $e->{filter}->{content};
                                     
              if ( defined($type) && defined($setval) )
                {
                  $MainType=set_filter_type($type,$setval);
                }
              else
                {
                  print "Main -> Filter type or content not defined. INVALID filter. \n" if $verbose;
                  $MainType = "invalid";
                }

              if ( $MainType eq 'host' && ( not $MainType eq 'invalid'))
                {
                  print "DEBUG: Entering mono filter. Current type familly: host ($setval) \n" if $verbose;
                  if ( defined($type) && $type eq 'equipment' ) # Is this an equipement ?
                    {
                      print "Main -> Entering mono filter. Current type : equipment ($setval)\n" if $verbose;                                                          
                      my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where equipment like \''. $setval .'\' ;');
                      $Ged_Request->execute();
                      write_event_host_to_rss($Ged_Request);
                    }
                  else ###### At this point, if this is not an equipement filter, it can only be hostgroup.  
                    {
                      print "DEBUG: Entering mono filter. Current type : supposed to be hostgroup  ($setval) \n" if $verbose;
                      # I guess i have to build host list :)
                      @HostList = (@HostList,build_HostList($setval));
                      ##### Now XML to RSS File
                      my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where equipment like \''. $HostList[0] .'\' ';
                      foreach (@HostList) 
                        {
                          print "Main ->          HOST: $_ \n" if $verbose;
                          $Ged_Filtered_Request = $Ged_Filtered_Request .' OR equipment like \''. $_ .'\' ';
                        }
                      $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                      print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;
                      my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                      $Ged_Request->execute();
                      write_event_host_to_rss($Ged_Request);
                      $#HostList = -1;
                      $#HostTemplate = -1;
                    }
                }

              if ( $MainType eq 'service' && ( not $MainType eq 'invalid'))
                {
                  print "DEBUG: Entering mono filter. Current type familly: Service ($setval) \n" if $verbose;
                  if ( defined($type) && $type eq 'service' ) # Is this an equipement ?
                    {
                      print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                      my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where service like \''. $setval .'\' ;');
                      $Ged_Request->execute();

                      write_event_host_to_rss($Ged_Request);
                    }
                  else ###### At this point, if this is not an service filter, it can only be servicegroup.  
                    {
                      print "DEBUG: Entering mono filter. Current type : supposed to be servicegroup  ($setval) \n" if $verbose;
                      # I guess i have to build host list :)
                      @ServiceList = (@ServiceList,build_HostList($setval));                                                     
                                                        
                      ##### Now XML to RSS File
                      my $Ged_Filtered_Request = 'select id,equipment,service,description from nagios_queue_active where service like \''. $ServiceList[0] .'\' ';
                      foreach (@ServiceList) 
                        {
                          print "Main ->          SERVICE: $_ \n" if $verbose;
                          $Ged_Filtered_Request = $Ged_Filtered_Request .' OR service like \''. $_ .'\' ';
                        }
                      $Ged_Filtered_Request = $Ged_Filtered_Request . ';';
                      print "Main -> Ged DB Request:  $Ged_Filtered_Request \n" if $verbose;
                      my $Ged_Request = $db_ged->prepare($Ged_Filtered_Request);
                      $Ged_Request->execute();

                      write_event_host_to_rss($Ged_Request);
                    }
                }

              if ( $MainType eq 'description' )
                {
                  print "DEBUG: Entering multi-filter multi-user. Current type familly: Description ($setval) \n" if $verbose;
                  print "Main -> Entering multi-filter multi-user. Current type : service ($setval)\n" if $verbose;                                                          
                  my $Ged_Request = $db_ged->prepare('select id,equipment,service,description from nagios_queue_active where description like \''. $setval .'\' ;');
                  $Ged_Request->execute();

                  write_event_host_to_rss($Ged_Request);
                }

              print FileRSS "</channel> </rss> \n";
              close FileRSS;
            }       
        }
}

$db_ged->disconnect();
exit $ERRORS{'OK'};
