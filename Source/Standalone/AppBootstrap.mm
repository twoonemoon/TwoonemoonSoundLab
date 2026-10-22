// SPDX-License-Identifier: GPL-3.0-or-later
// Запуск standalone-приложения без MainMenu.nib.
//
// Обычно iPlug2 создаёт главное меню и SWELLAppController из .xib, который компилируется
// утилитой ibtool (входит только в полный Xcode). Чтобы сборка работала с одними
// Command Line Tools и была воспроизводимой, меню и делегат создаются программно
// до завершения запуска NSApplication.
#if defined(APP_API) && defined(__APPLE__)

#import <Cocoa/Cocoa.h>

#include "resource.h"

namespace {

NSMenuItem* addItem(NSMenu* menu, NSString* title, SEL action, NSString* key, id target, NSInteger tag)
{
  NSMenuItem* item = [menu addItemWithTitle:title action:action keyEquivalent:key ? key : @""];
  if (target) [item setTarget:target];
  if (tag) [item setTag:tag];
  return item;
}

void installMenuAndDelegate()
{
  if ([NSApp delegate] != nil) return; // nib всё-таки загружен — ничего не делаем

  Class controllerClass = NSClassFromString(@"SWELLAppController");
  if (!controllerClass) return;
  id controller = [[controllerClass alloc] init]; // живёт всё время работы приложения

  NSString* appName = [[NSProcessInfo processInfo] processName];
  NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];

  // Меню приложения
  NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:appName action:nil keyEquivalent:@""];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:appName];
  addItem(appMenu, [@"О программе " stringByAppendingString:appName], @selector(onSysMenuCommand:), nil, controller, ID_ABOUT);
  [appMenu addItem:[NSMenuItem separatorItem]];
  addItem(appMenu, @"Настройки аудио…", @selector(onSysMenuCommand:), @",", controller, ID_PREFERENCES);
  [appMenu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* servicesItem = addItem(appMenu, @"Службы", nil, nil, nil, 0);
  NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
  [servicesItem setSubmenu:servicesMenu];
  [NSApp setServicesMenu:servicesMenu];
  [appMenu addItem:[NSMenuItem separatorItem]];
  addItem(appMenu, [@"Скрыть " stringByAppendingString:appName], @selector(hide:), @"h", nil, 0);
  NSMenuItem* hideOthers = addItem(appMenu, @"Скрыть остальные", @selector(hideOtherApplications:), @"h", nil, 0);
  [hideOthers setKeyEquivalentModifierMask:NSEventModifierFlagOption | NSEventModifierFlagCommand];
  addItem(appMenu, @"Показать все", @selector(unhideAllApplications:), nil, nil, 0);
  [appMenu addItem:[NSMenuItem separatorItem]];
  addItem(appMenu, [@"Завершить " stringByAppendingString:appName], @selector(terminate:), @"q", nil, ID_QUIT);
  [appItem setSubmenu:appMenu];
  [mainMenu addItem:appItem];

  // Меню «Окно»
  NSMenuItem* windowItem = [[NSMenuItem alloc] initWithTitle:@"Окно" action:nil keyEquivalent:@""];
  NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Окно"];
  addItem(windowMenu, @"Свернуть", @selector(performMiniaturize:), @"m", nil, 0);
  addItem(windowMenu, @"Масштаб окна", @selector(performZoom:), nil, nil, 0);
  [windowMenu addItem:[NSMenuItem separatorItem]];
  addItem(windowMenu, @"Все окна — на передний план", @selector(arrangeInFront:), nil, nil, 0);
  [windowItem setSubmenu:windowMenu];
  [mainMenu addItem:windowItem];

  [NSApp setMainMenu:mainMenu];
  [NSApp setWindowsMenu:windowMenu];

  // То же, что делает nib: SWELLAppController назначает себя делегатом и шлёт SWELLAPP_ONLOAD
  if ([controller respondsToSelector:@selector(awakeFromNib)])
    [controller performSelector:@selector(awakeFromNib)];
}

} // namespace

__attribute__((constructor)) static void TSLRegisterAppBootstrap()
{
  @autoreleasepool
  {
    [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationWillFinishLaunchingNotification
                                                      object:nil
                                                       queue:nil
                                                  usingBlock:^(NSNotification*) { installMenuAndDelegate(); }];
  }
}

#endif
