Данное приложение демонстрирует работу сети.
JetOS по сети начинает общаться с скриптом, который выполняется
на инструментальной машине.
Поддерживаются два варианта запуcка: на железе и в QEMU.

Для запуска в QEMU:
 1. Создать tap устройство c именем tap0. Для этого выполнить команду
   './misc/setup-tap_nobr.sh 1' в корневой директории JetOS

 2. Узнать mac адрес tap0. Для этого можно выполнить 'ifconfig tap0' и
   посмотреть значение 'ether'.

 3. Прописать mac адрес tap0 в JetOS:
   в файле 'P2/components-glue/config.yaml' в компоненте 'udp_ip_sender_1'
   поменять 'dst_mac'. После этого необходимо выполнить 'scons
   cdeveloper=1 bsp=<нужный bsp>'

 4. запустить jetos используя команду 'scons run-tap'

 5. в отдельном терминале перейти в корень JetOS и выполнить
   'sudo ifconfig tap0 192.168.56.1 && sudo ifconfig tap0 up && python3 misc/net_demo.py'.
   После запуска в появившемся нем должна отображаться скорость сети.


Возможные проблемы:
 - т.к. virtio находится на PCI шине, а PCI в JetOS статически конфигурируется
   то, если в системе стоит неправильный адрес pci устройства, то приложение
   работать не будет работать не будет. Чтоб исправить надо заменить в файле
   'P2/components-glue/config.yaml' в поле 'state' компонента 'net_dev_1'
   pci_bus, pci_dev, pci_fn на нужные. Правильные значения можно узнать
   выполнив команду 'info pci'


Для запуска на железе необходимо:
0. Изменить конфигурацию системного раздела, чтоб он использовал dtsec драйвер
   вместо virtio. Для этого нужно в файле 'P2/components-glue/config.yaml'
     a. закоментировать или удалить
            - name: net_dev_1
              type: VIRTIO_NET_DEV
              state:
                 pci_bus: 0
                 pci_dev: 2
                 pci_fn: 0

     b. разкоментировать
            - name: net_dev_1
              type: DTSEC_NET_DEV
              state:
                  dtsec_num: 3


1. Выставить destination mac адрес (mac адрес инструментальной машины). Для
   этого необходимо в файле 'P2/components-glue/config.yaml' в компоненте
   'udp_ip_sender_1' поменять 'dst_mac'. После этого необходимо выполнить 'scons
   cdeveloper=1 bsp=<нужный bsp>'

2. запустить на целевом железе JetOS

3. На инструментальной машине сетевой карте, к которой подключена железка прописать
   ip адрес 192.168.56.1. После выполнить 'python3 misc/net_demo.py'.

   Линукс на инструментально машине может сбрасывать ip адрес при перезагрузке
   подлкюченной железки, если это произошло, то надо востановить ip адрес

запустить на инструментальной машине 'python3 misc/net_demo.py'. После запуска в нем должна
   отображаться скорость сети.

