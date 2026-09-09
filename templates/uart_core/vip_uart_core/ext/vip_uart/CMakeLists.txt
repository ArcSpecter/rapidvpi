# vip_uart - reusable UART protocol VIP for RapidVPI/C++ coroutines.
#
# Included under <project>/ext/vip_uart. Builds object libraries and
# contributes them to the parent ${PROJECT_NAME} shared-library target.

add_subdirectory(common)
add_subdirectory(agents/uart_tx)
add_subdirectory(agents/uart_rx)
add_subdirectory(scoreboard/uart_scb)
