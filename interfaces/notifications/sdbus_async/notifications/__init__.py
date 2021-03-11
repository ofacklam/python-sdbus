# SPDX-License-Identifier: LGPL-2.1-or-later

# Copyright (C) 2020, 2021 igo95862

# This file is part of python-sdbus

# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

from sdbus import (DbusInterfaceCommonAsync, dbus_method_async,
                   dbus_signal_async)
from sdbus.sd_bus_internals import SdBus


class NotificationsInterface(
        DbusInterfaceCommonAsync,
        interface_name='org.freedesktop.Notifications'):

    @dbus_method_async('u')
    async def close_notification(self, notif_id: int) -> None:
        raise NotImplementedError

    @dbus_method_async()
    async def get_capabilities(self) -> List[str]:
        raise NotImplementedError

    @dbus_method_async()
    async def get_server_infomation(self) -> Tuple[str, str, str, str]:
        raise NotImplementedError

    @dbus_method_async("susssasa{sv}i")
    async def notify(
            self,
            app_name: str = '',
            replaces_id: int = 0,
            app_icon: str = '',
            summary: str = '',
            body: str = '',
            actions: List[str] = [],
            hints: Dict[str, Tuple[str, Any]] = {},
            expire_timeout: int = -1, ) -> int:

        raise NotImplementedError

    @dbus_signal_async()
    def action_invoked(self) -> Tuple[int, int]:
        raise NotImplementedError

    @dbus_signal_async()
    def notification_closed(self) -> Tuple[int, int]:
        raise NotImplementedError

    def create_hints(
        self,
        use_action_icons: Optional[bool] = None,
        category: Optional[str] = None,
        desktop_entry_name: Optional[str] = None,
        image_data_tuple: Optional[
            Tuple[int, int, int, bool, int, int, Union[bytes, bytearray]]
        ] = None,
        image_path: Optional[Union[str, Path]] = None,
        is_resident: Optional[bool] = None,
        sound_file_path: Optional[Union[str, Path]] = None,
        sound_name: Optional[str] = None,
        suppress_sound: Optional[bool] = None,
        is_transient: Optional[bool] = None,
        xy_pos: Optional[Tuple[int, int]] = None,
        urgency: Optional[int] = None,
    ) -> Dict[str, Tuple[str, Any]]:

        hints_dict: Dict[str, Tuple[str, Any]] = {}

        # action-icons
        if use_action_icons is not None:
            hints_dict['action-icons'] = ('b', use_action_icons)

        # category
        if category is not None:
            hints_dict['category'] = ('s', category)

        # desktop-entry
        if desktop_entry_name is not None:
            hints_dict['desktop-entry'] = ('s', desktop_entry_name)

        # image-data
        if image_data_tuple is not None:
            hints_dict['image-data'] = ('iiibiiay', image_data_tuple)

        # image-path
        if image_path is not None:
            hints_dict['image-path'] = (
                's',
                image_path
                if isinstance(image_path, str)
                else str(image_path))

        # resident
        if is_resident is not None:
            hints_dict['resident'] = ('b', is_resident)

        # sound-file
        if sound_file_path is not None:
            hints_dict['sound-file'] = (
                's',
                sound_file_path
                if isinstance(sound_file_path, str)
                else str(sound_file_path))

        # sound-name
        if sound_name is not None:
            hints_dict['sound-name'] = (
                's', sound_name
            )

        # suppress-sound
        if suppress_sound is not None:
            hints_dict['suppress-sound'] = ('b', suppress_sound)

        # is_transient
        if is_transient is not None:
            hints_dict['is_transient'] = ('b', is_transient)

        # x
        # y
        if xy_pos is not None:
            hints_dict['x'] = ('i', xy_pos[0])
            hints_dict['y'] = ('i', xy_pos[1])

        return hints_dict


class FreedesktopNotifications(NotificationsInterface):
    def __init__(self, bus: Optional[SdBus] = None) -> None:
        super().__init__()
        self._connect(
            'org.freedesktop.Notifications',
            '/org/freedesktop/Notifications',
            bus,
        )
