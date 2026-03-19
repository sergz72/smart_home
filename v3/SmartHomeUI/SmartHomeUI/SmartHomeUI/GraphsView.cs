using System.Collections.Generic;
using Eto.Forms;

namespace SmartHomeUI;

internal sealed class Graph : StackLayout
{
    internal Graph()
    {
        Orientation = Orientation.Horizontal;
    }
}

internal abstract class GraphsView: StackLayout
{
    protected readonly Graph[] _graphs;
    
    protected GraphsView(int graphsCount)
    {
        Orientation = Orientation.Vertical;
        _graphs = new Graph[graphsCount];
        for (var i = 0; i < graphsCount; i++)
        {
            _graphs[i] = new Graph();
            Items.Add(_graphs[i]);
        }
    }

    internal abstract void Refresh(Dictionary<string, Dictionary<int, SensorData>> data);
}

internal sealed class EnvGraphsView: GraphsView
{
    private readonly Graph _intTempGraph;
    private readonly Graph _extTempGraph;
    private readonly Graph _humiGraph;
    private readonly Graph _presGraph;
    private readonly Graph _co2Graph;
    private readonly Graph _luxGraph;
    internal EnvGraphsView(): base(6)
    {
        _intTempGraph = _graphs[0];
        _extTempGraph = _graphs[1];
        _humiGraph = _graphs[2];
        _presGraph = _graphs[3];
        _co2Graph = _graphs[4];
        _luxGraph = _graphs[5];
    }

    internal override void Refresh(Dictionary<string, Dictionary<int, SensorData>> data)
    {
    }
}

internal sealed class WatGraphsView: GraphsView
{
    private readonly Graph _pressureGraph;
    private readonly Graph _currentGraph;
    internal WatGraphsView(): base(2)
    {
        _pressureGraph = _graphs[0];
        _currentGraph = _graphs[1];
    }

    internal override void Refresh(Dictionary<string, Dictionary<int, SensorData>> data)
    {
    }
}

internal sealed class EleGraphsView: GraphsView
{
    private readonly Graph _powerGraph;
    private readonly Graph _voltageGraph;
    internal EleGraphsView(): base(2)
    {
        _powerGraph = _graphs[0];
        _voltageGraph = _graphs[1];
    }

    internal override void Refresh(Dictionary<string, Dictionary<int, SensorData>> data)
    {
    }
}