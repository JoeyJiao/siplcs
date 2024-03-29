//
//  ESSIPELibpurpleServicePlugin.m
//  SIPEAdiumPlugin
//
//  Created by Matt Meissner on 10/30/09.
//  Copyright 2009 Matt Meissner. All rights reserved.
//

#import <libpurple/libpurple.h>
#import "ESSIPEService.h"
#import "ESSIPELibpurpleServicePlugin.h"

#include "sipe-core.h"

extern void purple_init_sipe_plugin(void);

@implementation ESSIPELibpurpleServicePlugin

- (void)installLibpurplePlugin {}

- (void)loadLibpurplePlugin 
{
	purple_init_sipe_plugin();
}

- (void)installPlugin
{
	[super installPlugin];
	
	[ESSIPEService registerService];
}

- (void)dealloc
{
	[SIPEService release];
	[super dealloc];
}

- (NSString *)libpurplePluginPath
{
	return [[NSBundle bundleForClass:[self class]] resourcePath];
}

@end
