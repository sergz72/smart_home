using System;
using Eto.Forms;
using Eto.Drawing;

namespace SmartHomeUI
{
    public sealed partial class MainForm : Form
    {
        private readonly TreeGridView _summaryView;
        private readonly ComboBox _offsetBox, _offsetTypeBox;
        private readonly ComboBox _periodBox, _periodTypeBox, _filterTypeBox;
        private readonly CheckBox _enablePeriodBox;
        private readonly ComboBox _startDay, _startMonth, _startYear;
        private readonly TabControl _tabControl;
        private readonly Label _statusLabel;
        private readonly StatusDataStore _summaryViewDataStore;
        private readonly SmartHomeService _service;
        private readonly GraphsView _envGraphsView, _watGraphsView, _eleGraphsView;

        public MainForm(SmartHomeService service)
        {
            _service = service;
            
            Title = "Smart home UI";
            MinimumSize = new Size(1900, 1000);

            _summaryViewDataStore = new StatusDataStore();
            
            _summaryView = new TreeGridView();
            _summaryView.Columns.Add(new GridColumn
            {
                DataCell = new TextBoxCell
                {
                    Binding = new DelegateBinding<StatusItem, string>(r => r.Text),
                }
            });
            _summaryView.DataStore = _summaryViewDataStore;
            
            _filterTypeBox = new ComboBox { Items = { "Offset", "Date" }, ReadOnly = true, SelectedIndex = 0 };
            _filterTypeBox.TextChanged += FilterTypeChanged;

            _offsetBox = new ComboBox { ReadOnly = true };
            for (var i = 1; i <= 30; i++)
                _offsetBox.Items.Add(i.ToString());
            _offsetBox.SelectedIndex = 0;

            _offsetTypeBox = new ComboBox { Items = { "Days", "Months", "Years" }, ReadOnly = true, SelectedIndex = 0 };
            
            _startDay = new ComboBox { ReadOnly = true, Enabled = false };
            for (var i = 1; i <= 31; i++)
                _startDay.Items.Add(i.ToString());
            _startDay.SelectedIndex = 0;
            
            _startMonth = new ComboBox { ReadOnly = true, Enabled = false,
                Items = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" }};
            _startMonth.SelectedIndex = 0;
            
            _startYear = new ComboBox { ReadOnly = true, Enabled = false,
                Items = { "2019", "2020", "2021", "2022", "2023", "2024", "2025", "2026", "2027", "2028", "2029", "2030" }};
            _startYear.SelectedIndex = 0;
            
            _periodBox = new ComboBox { ReadOnly = true, Enabled = false };
            for (var i = 1; i <= 30; i++)
                _periodBox.Items.Add(i.ToString());
            _periodBox.SelectedIndex = 0;
            
            _periodTypeBox = new ComboBox { Items = { "Days", "Months", "Years" }, ReadOnly = true, SelectedIndex = 0, Enabled = false };
            
            _enablePeriodBox = new CheckBox { Checked = false, Text = "Period"};
            _enablePeriodBox.CheckedChanged += (_, _) =>
            {
                var checkd = (bool)_enablePeriodBox.Checked;
                _periodBox.Enabled = checkd;
                _periodTypeBox.Enabled = checkd;
            };
            
            _statusLabel = new Label();

            _envGraphsView = new EnvGraphsView(service);
            _watGraphsView = new WatGraphsView(service);
            _eleGraphsView = new EleGraphsView(service);

            _tabControl = new TabControl
            {
                Pages =
                {
                    new TabPage
                    {
                        Text = "Summary",
                        Content = _summaryView
                    },
                    new TabPage
                    {
                        Text = "Environmental sensors",
                        Content = _envGraphsView
                    },
                    new TabPage
                    {
                        Text = "Water sensors",
                        Content = _watGraphsView
                    },
                    new TabPage
                    {
                        Text = "Electrical sensors",
                        Content = _eleGraphsView
                    }
                }
            };
            
            var refreshCommand = new Command { MenuText = "Refresh", ToolBarText = "Refresh" };
            refreshCommand.Executed += (_, _) => Refresh();

            Content = new StackLayout
            {
                Orientation = Orientation.Vertical,
                Items =
                {
                    new Panel
                    {
                        Content = new StackLayout
                        {
                            Orientation = Orientation.Horizontal,
                            Spacing = 10,
                            VerticalContentAlignment = VerticalAlignment.Center,
                            Items =
                            {
                                new Button { Text = "Refresh", Command = refreshCommand },
                                new Label { Text = "Filter type" },
                                _filterTypeBox,
                                new Label { Text = "Offset" },
                                _offsetBox,
                                _offsetTypeBox,
                                new Label { Text = "Start date" },
                                _startDay,
                                _startMonth,
                                _startYear,
                                _enablePeriodBox,
                                _periodBox,
                                _periodTypeBox
                            }
                        }
                    },
                    new StackLayoutItem
                    {
                        HorizontalAlignment = HorizontalAlignment.Stretch,
                        VerticalAlignment = VerticalAlignment.Stretch,
                        Expand = true,
                        Control = _tabControl
                    },
                    new Panel
                    {
                        Content = new StackLayout
                        {
                            Orientation = Orientation.Horizontal,
                            Spacing = 10,
                            VerticalContentAlignment = VerticalAlignment.Center,
                            Items =
                            {
                                _statusLabel
                            }
                        }
                    }
                }
            };

            var quitCommand = new Command
                { MenuText = "Quit", Shortcut = Application.Instance.CommonModifier | Keys.Q };
            quitCommand.Executed += (_, _) => Application.Instance.Quit();

            var aboutCommand = new Command { MenuText = "About..." };
            aboutCommand.Executed += (_, _) => new AboutDialog().ShowDialog(this);

            // create menu
            Menu = new MenuBar
            {
                Items =
                {
                    new SubMenuItem { Text = "&File", Items = { refreshCommand } },
                },
                QuitItem = quitCommand,
                AboutItem = aboutCommand
            };
        }
        
        private void FilterTypeChanged(object? sender, EventArgs e)
        {
            var cb = (ComboBox)sender!;
            var offset = cb.SelectedIndex == 0;
            _offsetBox.Enabled = offset;
            _offsetTypeBox.Enabled = offset;
            _startDay.Enabled = !offset;
            _startMonth.Enabled = !offset;
            _startYear.Enabled = !offset;
        }

        private void Refresh()
        {
            switch (_tabControl.SelectedIndex)
            {
                case 0: // Summary page
                    UpdateSummaryPage();
                    break;
                case 1: // Environmental sensors page
                    UpdateSensorsPage("env", _envGraphsView);
                    break;
                case 2: // Water sensors page
                    UpdateSensorsPage("wat", _watGraphsView);
                    break;
                default: // Electrical sensors page
                    UpdateSensorsPage("ele", _eleGraphsView);
                    break;
            }
        }

        private void UpdateSensorsPage(string dataType, GraphsView view)
        {
            try
            {
                var result = _service.GetSensorData(BuildSensorDataQuery(dataType));
                view.Refresh(result);
                UpdateStatus();
            }
            catch (Exception e)
            {
                _statusLabel.Text = e.Message;
            }
        }

        private SmartHomeQuery BuildSensorDataQuery(string dataType)
        {
            var period = _enablePeriodBox.Checked ?? false ? BuildDateOffset(_periodBox, _periodTypeBox) : null;
            if (_filterTypeBox.SelectedIndex == 0) // offset
                return new SmartHomeQuery((short)Width, dataType, null, BuildDateOffset(_offsetBox, _offsetTypeBox), 
                     period);
            return new SmartHomeQuery((short)Width, dataType, BuildStartDate(), null, period); 
        }

        private DateTime BuildStartDate()
        {
            var day = _startDay.SelectedIndex + 1;
            var month = _startMonth.SelectedIndex + 1;
            var year = int.Parse(_startYear.SelectedKey);
            return new DateTime(year, month, day);
        }

        private static DateOffset BuildDateOffset(ComboBox offsetBox, ComboBox offsetTypeBox)
        {
            var n = offsetBox.SelectedIndex + 1;
            return offsetTypeBox.SelectedIndex switch
            {
                0 => // Days
                    new DateOffset(n, TimeUnit.Day),
                1 => // Months
                    new DateOffset(n, TimeUnit.Month),
                _ => new DateOffset(n, TimeUnit.Year)
            };
        }

        private void UpdateSummaryPage()
        {
            try
            {
                var result = _service.GetLastSensorData();
                _summaryViewDataStore.Update(result);
                _summaryView.ReloadData();
                UpdateStatus();
            }
            catch (Exception e)
            {
                _statusLabel.Text = e.Message;
            }
        }
        
        private void UpdateStatus()
        {
            _statusLabel.Text = $"Response time {_service.ResponseTimeMs} ms";
        }
    }
}
