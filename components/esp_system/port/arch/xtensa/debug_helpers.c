// Copyright 2015-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>

#include "sdkconfig.h"
#include "esp_types.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_debug_helpers.h"
#include "soc/soc_memory_layout.h"
#include "soc/cpu.h"
#include "esp_private/panic_internal.h"

#include "xtensa/xtensa_context.h"

#include "sdkconfig.h"

#include "esp_rom_sys.h"

bool IRAM_ATTR esp_backtrace_get_next_frame(esp_backtrace_frame_t *frame)
{
    //Use frame(i-1)'s BS area located below frame(i)'s sp to get frame(i-1)'s sp and frame(i-2)'s pc
    void *base_save = (void *)frame->sp;     //Base save area consists of 4 words under SP
    frame->pc = frame->next_pc;
    frame->next_pc = *((uint32_t *)(base_save - 16));     //If next_pc = 0, indicates frame(i-1) is the last frame on the stack
    frame->sp =  *((uint32_t *)(base_save - 12));

    //Return true if both sp and pc of frame(i-1) are sane, false otherwise
    return (esp_stack_ptr_is_sane(frame->sp) && esp_ptr_executable((void*)esp_cpu_process_stack_pc(frame->pc)));
}

static void IRAM_ATTR print_entry(uint32_t pc, uint32_t sp, bool panic)
{
    if (panic) {
        panic_print_str("0x");
        panic_print_hex(pc);
    } else {
        esp_rom_printf("0x%08X", pc);
    }
}

static void IRAM_ATTR print_str(const char* str, bool panic)
{
    if (panic) {
        panic_print_str(str);
    } else {
        esp_rom_printf(str);
    }
}

esp_err_t IRAM_ATTR esp_backtrace_print_from_frame(int depth, const esp_backtrace_frame_t* frame, bool panic)
{
    //Check arguments
    if (depth <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    //Initialize stk_frame with first frame of stack
    esp_backtrace_frame_t stk_frame = { 0 };
    memcpy(&stk_frame, frame, sizeof(esp_backtrace_frame_t));

    print_str("\r\n\r\nBacktrace:", panic);
    print_entry(esp_cpu_process_stack_pc(stk_frame.pc), stk_frame.sp, panic);
    print_str(" ", panic);

    //Check if first frame is valid
    bool corrupted = !(esp_stack_ptr_is_sane(stk_frame.sp) &&
                       (esp_ptr_executable((void *)esp_cpu_process_stack_pc(stk_frame.pc)) ||
                        /* Ignore the first corrupted PC in case of InstrFetchProhibited */
                        (stk_frame.exc_frame && ((XtExcFrame *)stk_frame.exc_frame)->exccause == EXCCAUSE_INSTR_PROHIBITED)));

    uint32_t i = (depth <= 0) ? INT32_MAX : depth;
    while (i-- > 0 && stk_frame.next_pc != 0 && !corrupted) {
        if (!esp_backtrace_get_next_frame(&stk_frame)) {    //Get previous stack frame
            corrupted = true;
        }
        print_entry(esp_cpu_process_stack_pc(stk_frame.pc), stk_frame.sp, panic);
        print_str(" ", panic);
    }

    //Print backtrace termination marker
    esp_err_t ret = ESP_OK;
    if (corrupted) {
        print_str(" |<-CORRUPTED", panic);
        ret =  ESP_FAIL;
    } else if (stk_frame.next_pc != 0) {    //Backtrace continues
        print_str(" |<-CONTINUES", panic);
    }
    print_str("\r\n\r\n", panic);
    return ret;
}

esp_err_t IRAM_ATTR esp_backtrace_print(int depth)
{
    //Initialize stk_frame with first frame of stack
    esp_backtrace_frame_t start = { 0 };
    esp_backtrace_get_start(&(start.pc), &(start.sp), &(start.next_pc));
    return esp_backtrace_print_from_frame(depth, &start, false);
}
