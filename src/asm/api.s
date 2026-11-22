.module api

.globl spi_initialize
.globl _spi_send
.globl spi_exchange_nowait
.globl spi_exchange
.globl spi_clear_bit1
.globl spi_set_bit1
.globl spi_set_bit0
.globl sd_read_blocks
.globl flash_erase
.globl fifo_receive_busy
.globl fifo_transmit_busy
.globl fifo_receive
.globl fifo_send
.globl sd_read_to_cartridge
.globl launch_game
.globl read_register3
.globl select_mode3_bank
.globl select_mode1_bank
.globl sd_write_blocks
.globl cartridge_noop
.globl command_monitor
.globl set_register0
.globl set_register1
.globl fill_window
.globl check_window
.globl window_copy_program
.globl window_copy
.globl alternate_boot
.globl flash_restore_boot
.globl flash_patch_boot
.globl _sd_send
.globl _sd_exchange_nowait
.globl _sd_exchange
.globl _sd_select
.globl _sd_deselect
.globl _sd_write_blocks
.globl _sd_enable_fast
.globl _sd_read_blocks
.globl _sd_read_banks
.globl _spi_initialize
.globl _select_mode3_bank
.globl _read_register3
.globl _select_mode1_bank
.globl _fill_window
.globl _check_window
.globl _alternate_boot
.globl _flash_restore_boot
.globl _flash_patch_boot
.globl _launch_game
.globl _set_register0
.globl _set_register1
.globl _fifo_receive_busy
.globl _fifo_receive
.globl _fifo_send
.globl _command_monitor
.globl _flash_erase
.globl _window_copy_program
.globl _window_copy

; Context-compiled C callees; names describe observed behavior, not original symbols.
.globl _file_browser
.globl _sd_command
.globl _fat_find_path
_file_browser = 0x6d8d
_sd_command = 0x0975
_fat_find_path = 0x22f8

.globl _input_keys
.globl _key_repeat_counter
_input_keys = 0xd85d
_key_repeat_counter = 0xd9cd

.globl _input_header_marker
.globl _input_mix_counter
.globl _bios_version
.globl _os_version
_input_mix_counter = 0xc054

.globl _display_cursor
.globl _display_attributes
.globl _display_blink_counter
.globl _browser_previous_selection
.globl _browser_file
.globl _browser_history
.globl _screen_buffer
_display_cursor = 0xd9c7
_display_attributes = 0xd9c9
_display_blink_counter = 0xd85b
_browser_previous_selection = 0xd9cb
_browser_file = 0xd9ce
_browser_history = 0xdab3
_screen_buffer = 0xd85f

.globl _fat_date
.globl _fat_time

.globl _os_boot_mode
.globl _os_byte
.globl _os_work_buffer
.globl _cgb_detected
.globl _os_patch_source
_os_byte = 0xd858
_os_work_buffer = 0xd859
_cgb_detected = 0xd9ca

; Observed filesystem RAM layout; ordinary extern declarations preserve codegen.
.globl _fat_entry_count
.globl _fat_table_start
.globl _fat_volume_start
.globl _fat_cached_table_sector
.globl _fat_table_sectors
.globl _fat_root_sector
.globl _fat_data_start
.globl _fat_reserved_sectors
.globl _fat_sectors_per_cluster
.globl _fat_type
.globl _fat_cluster_shift
.globl _fat_cluster_cursor
.globl _fat_parent_cluster
.globl _fat_directory_count
.globl _fat_directory_is_root
.globl _fat_table_buffer
.globl _fat_work_buffer
.globl _fat_sector_scratch
.globl _fat_cluster_scratch
.globl _fat_file_state
_fat_entry_count = 0xc006
_fat_table_start = 0xc00a
_fat_volume_start = 0xc00e
_fat_cached_table_sector = 0xc016
_fat_table_sectors = 0xc01a
_fat_root_sector = 0xc01e
_fat_data_start = 0xc022
_fat_reserved_sectors = 0xc026
_fat_sectors_per_cluster = 0xc028
_fat_type = 0xc029
_fat_cluster_shift = 0xc02a
_fat_cluster_cursor = 0xc02b
_fat_parent_cluster = 0xc02f
_fat_directory_count = 0xc033
_fat_directory_is_root = 0xc035
_fat_table_buffer = 0xc038
_fat_work_buffer = 0xc03a
_fat_sector_scratch = 0xc04c
_fat_cluster_scratch = 0xc050
_fat_file_state = 0xc03c

; Downward-growing pool and its 16-bit remaining-byte counter.
.globl _heap_pool
.globl _heap_remaining
_heap_pool = 0xc056
_heap_remaining = 0xd856

.area CARTRIDGE_API (ABS)
.org 0x0216
_spi_initialize:
    jp spi_initialize
_sd_send:
    jp _spi_send
_sd_exchange_nowait:
    jp spi_exchange_nowait
_sd_exchange:
    jp spi_exchange
_sd_select:
    jp spi_clear_bit1
_sd_deselect:
    jp spi_set_bit1
_sd_enable_fast:
    jp spi_set_bit0
_sd_read_blocks:
    jp sd_read_blocks
_flash_erase:
    jp flash_erase
_fifo_receive_busy:
    jp fifo_receive_busy
    jp fifo_transmit_busy
_fifo_receive:
    jp fifo_receive
_fifo_send:
    jp fifo_send
_sd_read_banks:
    jp sd_read_to_cartridge
_launch_game:
    jp launch_game
_read_register3:
    jp read_register3
_select_mode3_bank:
    jp select_mode3_bank
_select_mode1_bank:
    jp select_mode1_bank
_sd_write_blocks:
    jp sd_write_blocks
    jp cartridge_noop
_command_monitor:
    jp command_monitor
_set_register0:
    jp set_register0
_set_register1:
    jp set_register1
_fill_window:
    jp fill_window
_check_window:
    jp check_window
_window_copy_program:
    jp window_copy_program
_window_copy:
    jp window_copy
_alternate_boot:
    jp alternate_boot
_flash_restore_boot:
    jp flash_restore_boot
_flash_patch_boot:
    jp flash_patch_boot
